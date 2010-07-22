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

function should_be_invalid_template(t)
{
    var repo = sg.open_repo(reponame);
    var zs = new zingdb(repo, sg.dagnum.TESTING_DB);
    var ztx = zs.begin_tx();
    var err = "";
    try
    {
        ztx.set_template(t);
    }
    catch (e)
    {
        err = e.toString();
    }
    ztx.abort();
    err_must_contain(err, "Invalid template");
    repo.close();
    return err;
}

function st_zing_templates()
{
    this.reponame = null;

    this.setUp = function()
    {
        reponame = sg.gid();
        sg.new_repo(reponame, false);
    }

    this.test_no_version = function()
    {
        var t =
        {
            //"version" : 1,
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
                                "required" : true,
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "missing");
        err_must_contain(err, "version");
    }

    this.test_version_too_high = function()
    {
        var t =
        {
            "version" : 2,
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
                                "required" : true,
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);

        err_must_contain(err, "version");
        err_must_contain(err, "unsupported");
    }

    this.test_invalid_top_level_item = function()
    {
        var t =
        {
            "version" : 1,
            "fried" : 22,
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
                                "required" : true,
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
    }

    this.test_rectypes_not_an_object = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" : 22
        };
        var err = should_be_invalid_template(t);

    }

    this.test_invalid_type_for_allowed_element = function()
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
                                "allowed" : [34, 22, "nevermind"]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);

    }

    this.test_int_min_gt_max = function()
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
                                "min" : 12,
                                "max" : 9
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "max");
        err_must_contain(err, "min");

    }

    this.test_int_allowed_dup = function()
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
                                "allowed" : [5, 7, 9, 5]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "dup");
        err_must_contain(err, "allowed");

    }

    this.test_int_prohibited_dup = function()
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
                                "prohibited" : [5, 7, 9, 5]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "dup");
        err_must_contain(err, "prohibited");

    }

    this.test_int_allowed_gt_max = function()
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
                                "min" : 4,
                                "max" : 8,
                                "allowed" : [5, 7, 9]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "max");
        err_must_contain(err, "allowed");

    }

    this.test_int_allowed_lt_min = function()
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
                                "min" : 4,
                                "max" : 8,
                                "allowed" : [5, 7, 3]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "min");
        err_must_contain(err, "allowed");

    }

    this.test_int_defaultvalue_gt_max = function()
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
                                "defaultvalue" : 8,
                                "max" : 5
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "max");
        err_must_contain(err, "default");

    }

    this.test_int_defaultvalue_lt_min = function()
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
                                "defaultvalue" : 8,
                                "min" : 35
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "min");
        err_must_contain(err, "default");

    }

    this.test_int_defaultvalue_not_allowed = function()
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
                                "defaultvalue" : 8,
                                "allowed" : [5, 7, 9]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "allowed");
        err_must_contain(err, "default");

    }

    this.test_int_defaultvalue_prohibited = function()
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
                                "defaultvalue" : 6,
                                "prohibited" : [5, 7, 9, 6]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "prohibited");
        err_must_contain(err, "default");

    }

    this.test_int_allowed_and_prohibited = function()
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
                                "allowed" : [3,5,7,9],
                                "prohibited" : [2,4,6,8]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "prohibited");
        err_must_contain(err, "allowed");

    }

    this.test_int_allowed_empty = function()
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
                                "defaultvalue" : 77,
                                "allowed" : []
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "allowed");

    }

    this.test_field_name_too_long = function()
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
                        "iknewthebridewhensheusedtorockandroliknewthebridewhensheusedtorockandroliknewthebridewhensheusedtorockandroliknewthebridewhensheusedtorockandroliknewthebridewhensheusedtorockandroll" :
                        {
                            "datatype" : "string",
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "name");
    }

    this.test_string_minlength_gt_maxlength = function()
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
                                "minlength" : 12,
                                "maxlength" : 9
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "min");
        err_must_contain(err, "max");

    }

    this.test_string_allowed_dup = function()
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
                                "allowed" : ["hello", "thisistoolong", "hello"]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "duplicate");
        err_must_contain(err, "allowed");

    }

    this.test_string_allowed_gt_maxlength = function()
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
                                "minlength" : 4,
                                "maxlength" : 8,
                                "allowed" : ["hello", "thisistoolong"]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "max");
        err_must_contain(err, "allowed");

    }

    this.test_string_allowed_lt_minlength = function()
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
                                "minlength" : 4,
                                "maxlength" : 8,
                                "allowed" : ["hi", "this"]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "min");
        err_must_contain(err, "allowed");

    }

    this.test_string_defaultvalue_longer_than_maxlength = function()
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
                                "defaultvalue" : "howdy",
                                "maxlength" : 4
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "max");
        err_must_contain(err, "default");

    }

    this.test_string_defaultvalue_shorter_than_minlength = function()
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
                                "defaultvalue" : "howdy",
                                "minlength" : 12
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "min");
        err_must_contain(err, "default");

    }

    this.test_string_defaultvalue_not_allowed = function()
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
                                "defaultvalue" : "howdy",
                                "allowed" : ["hi", "this"]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "allowed");
        err_must_contain(err, "default");

    }

    this.test_string_defaultvalue_prohibited = function()
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
                                "defaultvalue" : "this",
                                "prohibited" : ["hi", "this"]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "prohibited");
        err_must_contain(err, "default");

    }

    this.test_string_allowed_and_prohibited = function()
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
                                "allowed" : ["hi", "this"],
                                "prohibited" : ["never", "mind"]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "prohibited");
        err_must_contain(err, "allowed");

    }

    this.test_string_prohibited_dup = function()
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
                                "prohibited" : ["hello", "thisistoolong", "hello"]
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "duplicate");
        err_must_contain(err, "prohib");

    }

    this.test_string_allowed_empty = function()
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
                                "defaultvalue" : "howdy",
                                "allowed" : []
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "allowed");

    }

    this.test_invalid_type_for_optional_item = function()
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
                                "required" : 5,
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);

    }

    this.test_invalid_type_for_field = function()
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
                        "well" : "hello",
                        "val" :
                        {
                            "datatype" : "int",
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);

    }

    this.test_rectypes_missing = function()
    {
        var t =
        {
            "version" : 1,
        };
        var err = should_be_invalid_template(t);

    }

    this.test_no_constraints = function()
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
                        }
                    }
                }
            }
        };

        var repo = sg.open_repo(reponame);
        var zs = new zingdb(repo, sg.dagnum.TESTING_DB);
        var ztx = zs.begin_tx();
        var err = "";
        try
        {
            ztx.set_template(t);
        }
        catch (e)
        {
            err = e.toString();
        }
        ztx.abort();
        testlib.ok(0 == err.length, "constraints on a field are optional");
        repo.close();
    }

    this.test_invalid_rectype_name__space = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                " item" :
                {
                    "fields" :
                    {
                        "val" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "required" : true,
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);

    }

    this.test_invalid_rectype_name__starts_with_digit = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "7item" :
                {
                    "fields" :
                    {
                        "val" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "required" : true,
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "name");

    }

    this.test_invalid_rectype_name__contains_a_space = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item tracked" :
                {
                    "fields" :
                    {
                        "val" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "required" : true,
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "name");

    }

    this.test_link_rectypes_dup = function()
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
                        "foo" : 
                        { 
                            "datatype" : "int"
                        } 
                    }
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

        var err = should_be_invalid_template(t);
        err_must_contain(err, "duplicate");
    }

    this.test_link_name_field_name_clash = function()
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
                        "foo" : 
                        { 
                            "datatype" : "int"
                        } 
                    }
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
                            "name" : "foo",
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

        var err = should_be_invalid_template(t);
    }

    this.test_invalid_action_type = function()
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
                        "foo" : 
                        { 
                            "datatype" : "int"
                        } 
                    }
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
                        },
                        "actions" :
                        {
                            "folder sum" :
                            {
                                "type" : "perl",
                                "function" : "sum",
                                "field_from" : "val",
                                "field_to" : "sum"
                            }
                        }
                    }
            }
        };

        var err = should_be_invalid_template(t);
    }

    this.test_invalid_builtin_function_name = function()
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
                        "foo" : 
                        { 
                            "datatype" : "int"
                        } 
                    }
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
                        },
                        "actions" :
                        {
                            "folder sum" :
                            {
                                "type" : "builtin",
                                "function" : "fiddlesticks",
                                "field_from" : "val",
                                "field_to" : "sum"
                            }
                        }
                    }
            }
        };

        var err = should_be_invalid_template(t);
    }

    this.test_builtin_missing_field_spec = function()
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
                        "foo" : 
                        { 
                            "datatype" : "int"
                        } 
                    }
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
                        },
                        "actions" :
                        {
                            "folder sum" :
                            {
                                "type" : "builtin",
                                "function" : "sum",
                                "field_to" : "sum"
                            }
                        }
                    }
            }
        };

        var err = should_be_invalid_template(t);
    }

    this.test_rectype_doesnt_exist = function()
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
                                "dir"
                            ],
                            "name" : "children",
                            "singular" : false
                        }
                    }
            }
        };



        var err = should_be_invalid_template(t);
    }

    this.test_invalid_field_name__contains_a_space = function()
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
                        "value of field" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "required" : true,
                            }
                        }
                    }
                }
            }
        };
        var err = should_be_invalid_template(t);
        err_must_contain(err, "name");

    }

    this.test_dagnode_with_dagnum = function()
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
                        }
                    }
                }
            }
        };

        var err = should_be_invalid_template(t);
    }

    this.test_string_field = function()
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
                                "required" : true,
                                "defaultvalue" : "hello",
                                "allowed" : ["hola", "hello"]
                            }
                        }
                    }
                }
            }
        };

        var repo = sg.open_repo(reponame);
        var zs = new zingdb(repo, sg.dagnum.TESTING_DB);
        var ztx = zs.begin_tx();
        var err = "";
        try
        {
            ztx.set_template(t);
        }
        catch (e)
        {
            err = e.toString();
        }
        ztx.abort();
        testlib.ok(0 == err.length, "this template should have no errors");
        repo.close();
    }

    this.test_overwrite_template = function()
    {
        var t1 =
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

        var t2 =
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

        var repo = sg.open_repo(reponame);
        var zs = new zingdb(repo, sg.dagnum.TESTING_DB);

        var ztx = zs.begin_tx();
        ztx.set_template(t1);
        ztx.commit();

        var ztx = zs.begin_tx();
        ztx.set_template(t2);
        ztx.commit();

        var ztx = zs.begin_tx();
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.val = 73;
        ztx.commit();

        var ztx = zs.begin_tx();
        template = ztx.get_template();
        ztx.abort();

        repo.close();
        testlib.ok(true, "ok");
    }

    // TODO lots more tests can/should go here
}

