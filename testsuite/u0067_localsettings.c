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

/**
 * @file u0067_localsettings.c
 *
 * @details Tests sg_localsettings.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0067_localsettings)
#define MyDcl(name)				u0067_localsettings__##name
#define MyFn(name)				u0067_localsettings__##name

//////////////////////////////////////////////////////////////////

void MyFn(test__uninitialized_setting)(SG_context * pCtx)
{
    SG_bool dummyBool = SG_FALSE;
    double dummyDouble = 0;
    SG_enumeration dummyEnum = 0;
    SG_int64 dummyInt64 = 0;
    SG_string * pDummyString = NULL;
    SG_pathname * pDummyPathname = NULL;
    char * pDummyCharstar = NULL;
    SG_stringarray *pDummyStringarray = NULL;
    SG_local_setting_type dummyType = 0;

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_bool(pCtx, "u0067_nonexistent_setting", &dummyBool, NULL)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_double(pCtx, "u0067_nonexistent_setting", &dummyDouble, NULL)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_enumerated_string(pCtx, "u0067_nonexistent_setting", &dummyEnum, NULL)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_int(pCtx, "u0067_nonexistent_setting", &dummyInt64, NULL)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_string(pCtx, "u0067_nonexistent_setting", &pDummyString, NULL)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_string__pathname(pCtx, "u0067_nonexistent_setting", &pDummyPathname, NULL)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_string__sz(pCtx, "u0067_nonexistent_setting", &pDummyCharstar, NULL)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_stringarray(pCtx, "u0067_nonexistent_setting", &pDummyStringarray, NULL)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_type_of(pCtx, "u0067_nonexistent_setting", &dummyType)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__reset(pCtx, "u0067_nonexistent_setting", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );

    VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx, &pDummyString, "test")  );
    VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pDummyPathname, ".")  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_bool(pCtx, "u0067_nonexistent_setting", SG_FALSE, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_double(pCtx, "u0067_nonexistent_setting", 0, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_int(pCtx, "u0067_nonexistent_setting", 0, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string(pCtx, "u0067_nonexistent_setting", pDummyString, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string__pathname(pCtx, "u0067_nonexistent_setting", pDummyPathname, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string__sz(pCtx, "u0067_nonexistent_setting", "test", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST  );

    // Fall through to common cleanup.
fail:
    SG_STRING_NULLFREE(pCtx, pDummyString);
    SG_PATHNAME_NULLFREE(pCtx, pDummyPathname);
}

void MyFn(test__already_initialized)(SG_context * pCtx)
{
	SG_string * pDummyString = NULL;
	SG_pathname * pDummyPathname = NULL;

	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx, &pDummyString, "Bad News")  );
	VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pDummyPathname, "./badnews.txt")  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_string(pCtx, SG_LOCALSETTING__USERID, pDummyString, NULL)  ,  SG_ERR_ALREADY_INITIALIZED  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_string__pathname(pCtx, SG_LOCALSETTING__USERID, pDummyPathname, NULL)  ,  SG_ERR_ALREADY_INITIALIZED  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_string__sz(pCtx, SG_LOCALSETTING__USERID, "Bad News", NULL)  ,  SG_ERR_ALREADY_INITIALIZED  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_bool(pCtx, SG_LOCALSETTING__USERID, SG_FALSE)  ,  SG_ERR_ALREADY_INITIALIZED  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_double(pCtx, SG_LOCALSETTING__USERID, 0, NULL)  ,  SG_ERR_ALREADY_INITIALIZED  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_int(pCtx, SG_LOCALSETTING__USERID, 0, NULL)  ,  SG_ERR_ALREADY_INITIALIZED  );

	VERIFY_ERR_CHECK(  SG_localsettings__initialize_bool(pCtx, "u0067_already_initialized_bool", SG_TRUE)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_bool(pCtx, "u0067_already_initialized_bool", SG_TRUE)  ,  SG_ERR_ALREADY_INITIALIZED  );

	VERIFY_ERR_CHECK(  SG_localsettings__initialize_double(pCtx, "u0067_already_initialized_double", 0, NULL)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_double(pCtx, "u0067_already_initialized_double", 0, NULL)  ,  SG_ERR_ALREADY_INITIALIZED  );

	VERIFY_ERR_CHECK(  SG_localsettings__initialize_int(pCtx, "u0067_already_initialized_int64", 0, NULL)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_int(pCtx, "u0067_already_initialized_int64", 0, NULL)  ,  SG_ERR_ALREADY_INITIALIZED  );

	// Fall through to common cleanup.
fail:
	SG_STRING_NULLFREE(pCtx, pDummyString);
	SG_PATHNAME_NULLFREE(pCtx, pDummyPathname);
}

SG_bool PathsEqual(SG_pathname **p, SG_pathname *p2, SG_context * pCtx)
{
	SG_bool equal;
	equal = strcmp(SG_pathname__sz(*p),SG_pathname__sz(p2))==0;
	SG_PATHNAME_NULLFREE(pCtx, *p);
	return equal;
}
SG_bool StringsEqual(SG_string **p, SG_string *p2, SG_context * pCtx)
{
	SG_bool equal;
	equal = strcmp(SG_string__sz(*p),SG_string__sz(p2))==0;
	SG_STRING_NULLFREE(pCtx, *p);
	return equal;
}
SG_bool SzEqual(char **p, const char*p2, SG_context * pCtx)
{
	SG_bool equal;
	equal = strcmp(*p,p2)==0;
	SG_NULLFREE(pCtx, *p);
	return equal;
}

void MyFn(test__nonvalidated_settings)(SG_context * pCtx)
{
	SG_bool b = SG_FALSE;
	double d = 0;
	int64 i = 0;
	SG_pathname * p = NULL;
	SG_string * s = NULL;
	char * z = "0";

	SG_local_setting_type t;

    SG_reason_for_local_setting_value rb = SG_REASON_FOR_LOCAL_SETTING__UNSPECIFIED;
	SG_reason_for_local_setting_value rd = SG_REASON_FOR_LOCAL_SETTING__UNSPECIFIED;
	SG_reason_for_local_setting_value ri = SG_REASON_FOR_LOCAL_SETTING__UNSPECIFIED;
	SG_reason_for_local_setting_value rp = SG_REASON_FOR_LOCAL_SETTING__UNSPECIFIED;
	SG_reason_for_local_setting_value rs = SG_REASON_FOR_LOCAL_SETTING__UNSPECIFIED;
	SG_reason_for_local_setting_value rz = SG_REASON_FOR_LOCAL_SETTING__UNSPECIFIED;

	SG_pathname * p0 = NULL;
	SG_pathname * p1 = NULL;
	SG_string * s0 = NULL;
	SG_string * s1 = NULL;

	VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &p0, "./path0.txt")  );
	VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &p1, "./path1.txt")  );
	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx, &s0, "0")  );
	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx, &s1, "1")  );

	VERIFY_ERR_CHECK(  SG_localsettings__initialize_bool            (pCtx, "u0067_b", SG_TRUE      )  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_double          (pCtx, "u0067_d", 1,       NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_int             (pCtx, "u0067_i", 1,       NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_string__pathname(pCtx, "u0067_p", p1,      NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_string          (pCtx, "u0067_s", s1,      NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_string__sz      (pCtx, "u0067_z", "1",     NULL)  );

	VERIFY_ERR_CHECK(  SG_localsettings__get_type_of(pCtx, "u0067_b", &t)  );
	VERIFY_COND("", t==SG_LOCAL_SETTING_TYPE__BOOL);
	VERIFY_ERR_CHECK(  SG_localsettings__get_type_of(pCtx, "u0067_d", &t)  );
	VERIFY_COND("", t==SG_LOCAL_SETTING_TYPE__DOUBLE);
	VERIFY_ERR_CHECK(  SG_localsettings__get_type_of(pCtx, "u0067_i", &t)  );
	VERIFY_COND("", t==SG_LOCAL_SETTING_TYPE__INT);
	VERIFY_ERR_CHECK(  SG_localsettings__get_type_of(pCtx, "u0067_p", &t)  );
	VERIFY_COND("", t==SG_LOCAL_SETTING_TYPE__STRING);
	VERIFY_ERR_CHECK(  SG_localsettings__get_type_of(pCtx, "u0067_s", &t)  );
	VERIFY_COND("", t==SG_LOCAL_SETTING_TYPE__STRING);
	VERIFY_ERR_CHECK(  SG_localsettings__get_type_of(pCtx, "u0067_z", &t)  );
	VERIFY_COND("", t==SG_LOCAL_SETTING_TYPE__STRING);

	VERIFY_ERR_CHECK(  SG_localsettings__get_bool            (pCtx, "u0067_b", &b, NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_double          (pCtx, "u0067_d", &d, NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_int           (pCtx, "u0067_i", &i, NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__pathname(pCtx, "u0067_p", &p, NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string          (pCtx, "u0067_s", &s, NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__sz      (pCtx, "u0067_z", &z, NULL)  );
	VERIFY_COND("", b==SG_TRUE);
	VERIFY_COND("", d==1);
	VERIFY_COND("", i==1);
	VERIFY_COND("", PathsEqual(&p,p1, pCtx));
	VERIFY_COND("", StringsEqual(&s,s1, pCtx));
	VERIFY_COND("", SzEqual(&z,"1", pCtx));
	VERIFY_ERR_CHECK(  SG_localsettings__get_bool            (pCtx, "u0067_b", &b, &rb)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_double          (pCtx, "u0067_d", &d, &rd)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_int           (pCtx, "u0067_i", &i, &ri)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__pathname(pCtx, "u0067_p", &p, &rp)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string          (pCtx, "u0067_s", &s, &rs)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__sz      (pCtx, "u0067_z", &z, &rz)  );
	VERIFY_COND("", b==SG_TRUE);
	VERIFY_COND("", d==1);
	VERIFY_COND("", i==1);
	VERIFY_COND("", PathsEqual(&p,p1, pCtx));
	VERIFY_COND("", StringsEqual(&s,s1, pCtx));
	VERIFY_COND("", SzEqual(&z,"1", pCtx));
    VERIFY_COND("", (rb&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (rd&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (ri&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (rp&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (rs&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (rz&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);

	VERIFY_ERR_CHECK(  SG_localsettings__set_bool            (pCtx, "u0067_b", SG_TRUE, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_double          (pCtx, "u0067_d", 1, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_int           (pCtx, "u0067_i", 1, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_string__pathname(pCtx, "u0067_p", p1, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_string          (pCtx, "u0067_s", s1, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_string__sz      (pCtx, "u0067_z", "1", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );

	VERIFY_ERR_CHECK(  SG_localsettings__get_bool            (pCtx, "u0067_b", &b, &rb)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_double          (pCtx, "u0067_d", &d, &rd)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_int           (pCtx, "u0067_i", &i, &ri)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__pathname(pCtx, "u0067_p", &p, &rp)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string          (pCtx, "u0067_s", &s, &rs)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__sz      (pCtx, "u0067_z", &z, &rz)  );
	VERIFY_COND("", b==SG_TRUE);
	VERIFY_COND("", d==1);
	VERIFY_COND("", i==1);
	VERIFY_COND("", PathsEqual(&p,p1, pCtx));
	VERIFY_COND("", StringsEqual(&s,s1, pCtx));
	VERIFY_COND("", SzEqual(&z,"1", pCtx));
    VERIFY_COND("", (rb&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (rd&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (ri&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (rp&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (rs&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (rz&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);

	VERIFY_ERR_CHECK(  SG_localsettings__set_bool            (pCtx, "u0067_b", SG_FALSE, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_double          (pCtx, "u0067_d", 0, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_int           (pCtx, "u0067_i", 0, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_string__pathname(pCtx, "u0067_p", p0, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_string          (pCtx, "u0067_s", s0, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__set_string__sz      (pCtx, "u0067_z", "0", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );

	VERIFY_ERR_CHECK(  SG_localsettings__get_bool            (pCtx, "u0067_b", &b, &rb)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_double          (pCtx, "u0067_d", &d, &rd)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_int           (pCtx, "u0067_i", &i, &ri)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__pathname(pCtx, "u0067_p", &p, &rp)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string          (pCtx, "u0067_s", &s, &rs)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__sz      (pCtx, "u0067_z", &z, &rz)  );
	VERIFY_COND("", b==SG_FALSE);
	VERIFY_COND("", d==0);
	VERIFY_COND("", i==0);
	VERIFY_COND("", PathsEqual(&p,p0, pCtx));
	VERIFY_COND("", StringsEqual(&s,s0, pCtx));
	VERIFY_COND("", SzEqual(&z,"0", pCtx));
    VERIFY_COND("", (rb&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (rd&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (ri&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (rp&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (rs&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);
	VERIFY_COND("", (rz&SG_REASON_FOR_LOCAL_SETTING__SGCLOSET)!=0);

	VERIFY_ERR_CHECK(  SG_localsettings__reset(pCtx, "u0067_b", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__reset(pCtx, "u0067_d", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__reset(pCtx, "u0067_i", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__reset(pCtx, "u0067_p", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__reset(pCtx, "u0067_s", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );
	VERIFY_ERR_CHECK(  SG_localsettings__reset(pCtx, "u0067_z", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  );

	VERIFY_ERR_CHECK(  SG_localsettings__get_bool            (pCtx, "u0067_b", &b, &rb)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_double          (pCtx, "u0067_d", &d, &rd)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_int           (pCtx, "u0067_i", &i, &ri)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__pathname(pCtx, "u0067_p", &p, &rp)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string          (pCtx, "u0067_s", &s, &rs)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__sz      (pCtx, "u0067_z", &z, &rz)  );
	VERIFY_COND("", b==SG_TRUE);
	VERIFY_COND("", d==1);
	VERIFY_COND("", i==1);
	VERIFY_COND("", PathsEqual(&p,p1, pCtx));
	VERIFY_COND("", StringsEqual(&s,s1, pCtx));
	VERIFY_COND("", SzEqual(&z,"1", pCtx));
    VERIFY_COND("", (rb&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (rd&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (ri&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (rp&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (rs&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
	VERIFY_COND("", (rz&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);

	// Fall through to common cleanup.
fail:
	SG_PATHNAME_NULLFREE(pCtx, p0);
	SG_PATHNAME_NULLFREE(pCtx, p1);
	SG_STRING_NULLFREE(pCtx, s0);
	SG_STRING_NULLFREE(pCtx, s1);
}

void MyFn(test__wrong_type_errors)(SG_context * pCtx)
{
	SG_bool b = SG_FALSE;
	double d = 0;
	int64 i = 0;
	SG_pathname * p = NULL;
	SG_string * s = NULL;
	char * z = "0";

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_bool(pCtx, SG_LOCALSETTING__USERID, &b, NULL)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_double(pCtx, SG_LOCALSETTING__USERID, &d, NULL)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_int(pCtx, SG_LOCALSETTING__USERID, &i, NULL)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_bool(pCtx, SG_LOCALSETTING__USERID, SG_FALSE, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_double(pCtx, SG_LOCALSETTING__USERID, 0, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_int(pCtx, SG_LOCALSETTING__USERID, 0, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );

	VERIFY_ERR_CHECK(  SG_localsettings__initialize_int(pCtx, "u0067_type_checked_int", 0, NULL)  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_string__pathname(pCtx, "u0067_type_checked_int", &p, NULL)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_string(pCtx, "u0067_type_checked_int", &s, NULL)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__get_string__sz(pCtx, "u0067_type_checked_int", &z, NULL)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );

	VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &p, ".")  );
	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx, &s, "test")  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string__pathname(pCtx, "u0067_type_checked_int", p, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string(pCtx, "u0067_type_checked_int", s, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string__sz(pCtx, "u0067_type_checked_int", "asdf", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE  );

	// Fall through to common cleanup.
fail:
	SG_PATHNAME_NULLFREE(pCtx, p);
	SG_STRING_NULLFREE(pCtx, s);
}

SG_bool Is1to5_d(double value)
{
	return 1<=value&&value<=5;
}
SG_bool Is1to5_i(SG_int64 value)
{
	return 1<=value&&value<=5;
}
SG_enumeration Is1to5_s(const char * pValue)
{
	if('1'<=*pValue&&*pValue<='5' && pValue[1]=='\0')
		return 1;
	return -1;
}

void MyFn(test__basic_validation)(SG_context * pCtx)
{
	double d = 0;
	int64 i = 0;
	SG_string * s = NULL;
	char * z = "0";

	SG_string * s3 = NULL;
	SG_string * s7 = NULL;

	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx, &s3, "3")  );
	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx, &s7, "7")  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_double(pCtx, "u0067_validated_d", 7, Is1to5_d)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_int(pCtx, "u0067_validated_i", 7, Is1to5_i)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_string(pCtx, "u0067_validated_s", s7, Is1to5_s)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_string__sz(pCtx, "u0067_validated_s", "7", Is1to5_s)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );

	VERIFY_ERR_CHECK(  SG_localsettings__initialize_double(pCtx, "u0067_validated_d", 3, Is1to5_d)  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_int(pCtx, "u0067_validated_i", 3, Is1to5_i)  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_string(pCtx, "u0067_validated_s", s3, Is1to5_s)  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_string__sz(pCtx, "u0067_validated_z", "3", Is1to5_s)  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_double(pCtx, "u0067_validated_d", 7, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_int(pCtx, "u0067_validated_i", 7, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string(pCtx, "u0067_validated_s", s7, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string__sz(pCtx, "u0067_validated_z", "7", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );

	VERIFY_ERR_CHECK(  SG_localsettings__get_double          (pCtx, "u0067_validated_d", &d, NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_int           (pCtx, "u0067_validated_i", &i, NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string          (pCtx, "u0067_validated_s", &s, NULL)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__sz      (pCtx, "u0067_validated_z", &z, NULL)  );
	VERIFY_COND("", d==3);
	VERIFY_COND("", i==3);
	VERIFY_COND("", StringsEqual(&s,s3, pCtx));
	VERIFY_COND("", SzEqual(&z,"3", pCtx));

	// Fall through to common cleanup.
fail:
	SG_STRING_NULLFREE(pCtx, s3);
	SG_STRING_NULLFREE(pCtx, s7);
}

void MyFn(test__string_enumeration__verbosity)(SG_context * pCtx)
{
	char * z = NULL;
    SG_reason_for_local_setting_value reason = SG_REASON_FOR_LOCAL_SETTING__UNSPECIFIED;

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_string__sz(pCtx, "u0067_verbosity", "asdf", SG_logging__enumerate_log_level)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_string__sz(pCtx, "u0067_verbosity", "verbose", SG_logging__enumerate_log_level)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string__sz(pCtx, "u0067_verbosity", "asdf", SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string__sz(pCtx, "u0067_verbosity", &z, &reason)  );
	VERIFY_COND("", SzEqual(&z,"verbose",pCtx));
	VERIFY_COND("", (reason&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);
fail:;
}

typedef enum {u0067_apple, u0067_pizza, u0067_sushi} u0067_food;

SG_enumeration EnumerateFood(const char * pValue)
{
	if( SG_stricmp(pValue,"apple")==0 )
		return u0067_apple;
	if( SG_stricmp(pValue,"pizza")==0 )
		return u0067_pizza;
	if( SG_stricmp(pValue,"sushi")==0 )
		return u0067_sushi;
	return -1;
}

void MyFn(test__string_enumeration__food)(SG_context * pCtx)
{
	SG_string * s = NULL;
	SG_reason_for_local_setting_value reason = SG_REASON_FOR_LOCAL_SETTING__UNSPECIFIED;

	SG_string * pizza = NULL;
	SG_string * motorcycle = NULL;

	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx, &pizza, "pizza")  );
	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx, &motorcycle, "motorcycle")  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__initialize_string(pCtx, "u0067_food", motorcycle, EnumerateFood)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK(  SG_localsettings__initialize_string(pCtx, "u0067_food", pizza, EnumerateFood)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_localsettings__set_string(pCtx, "u0067_food", motorcycle, SG_LOCAL_SETTING_SAVE_TO_SGCLOSET)  ,  SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING  );
	VERIFY_ERR_CHECK(  SG_localsettings__get_string(pCtx, "u0067_food", &s, &reason)  );
	VERIFY_COND("", StringsEqual(&s,pizza,pCtx));
	VERIFY_COND("", (reason&SG_REASON_FOR_LOCAL_SETTING__DEFAULT)!=0);

	// Fall through to common cleanup.
fail:
	SG_STRING_NULLFREE(pCtx, pizza);
	SG_STRING_NULLFREE(pCtx, motorcycle);
}

void MyFn(test__list)(SG_context * pCtx)
{
	SG_bool b = SG_FALSE;
	double d = 0;
	SG_int64 i = 0;
	SG_local_setting_type t = 0;

	const char ** a = NULL;
	SG_bool found_username = SG_FALSE;
	SG_int64 x = 0;

	VERIFY_ERR_CHECK(  SG_localsettings__list(pCtx, &a)  );

	while(a[x]!=NULL)
	{
		VERIFY_ERR_CHECK(  SG_localsettings__get_type_of(pCtx, a[x], &t)  );
		if(strcmp(a[x],SG_LOCALSETTING__USERID)==0)
		{
			VERIFY_COND("", !found_username);
			found_username = SG_TRUE;
			VERIFY_COND("", t==SG_LOCAL_SETTING_TYPE__STRING);
		}
		if(t==SG_LOCAL_SETTING_TYPE__BOOL)
			VERIFY_ERR_CHECK(  SG_localsettings__get_bool(pCtx, a[x], &b, NULL)  );
		else if(t==SG_LOCAL_SETTING_TYPE__DOUBLE)
			VERIFY_ERR_CHECK(  SG_localsettings__get_double(pCtx, a[x], &d, NULL)  );
		else if(t==SG_LOCAL_SETTING_TYPE__INT)
			VERIFY_ERR_CHECK(  SG_localsettings__get_int(pCtx, a[x], &i, NULL)  );
		else if(t==SG_LOCAL_SETTING_TYPE__STRING)
        {
            SG_string * s = NULL;
            VERIFY_ERR_CHECK(  SG_localsettings__get_string(pCtx, a[x], &s, NULL)  );
            SG_STRING_NULLFREE(pCtx, s);
        }
		++x;
	}

	VERIFY_COND("", found_username);

	// Fall through to common cleanup.
fail:
	SG_NULLFREE(pCtx, a);
}

void MyFn(test__list__vhash)(SG_context * pCtx)
{
    SG_vhash * pVhash = NULL;
    const char * pUserid = NULL;

    VERIFY_ERR_CHECK(  SG_localsettings__list__vhash(pCtx, &pVhash)  );
    VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhash, SG_LOCALSETTING__USERID, &pUserid)  );
    VERIFY_COND("", pUserid!=NULL);

    // Fall through to common cleanup.
fail:
    SG_VHASH_NULLFREE(pCtx, pVhash);
}

//////////////////////////////////////////////////////////////////

MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test__uninitialized_setting)(pCtx)  );
	BEGIN_TEST(  MyFn(test__already_initialized)(pCtx)  );
	BEGIN_TEST(  MyFn(test__nonvalidated_settings)(pCtx)  );
	BEGIN_TEST(  MyFn(test__wrong_type_errors)(pCtx)  );
	BEGIN_TEST(  MyFn(test__basic_validation)(pCtx)  );
	BEGIN_TEST(  MyFn(test__string_enumeration__verbosity)(pCtx)  );
	BEGIN_TEST(  MyFn(test__string_enumeration__food)(pCtx)  );
	BEGIN_TEST(  MyFn(test__list)(pCtx)  );
	BEGIN_TEST(  MyFn(test__list__vhash)(pCtx)  );

	TEMPLATE_MAIN_END;
}

//////////////////////////////////////////////////////////////////

#undef MyMain
#undef MyDcl
#undef MyFn
