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

// testsuite/unittests/u0014_pathname.c
// test pathname manipulation
//////////////////////////////////////////////////////////////////

// TODO add tests to verify that we properly fail when setting pathname with overlapping buffers.

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

int u0014_pathname__cwd(SG_context * pCtx)
{
	// test sg_pathname wrapper for cwd.

	SG_pathname * pPathname;

	VERIFY_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC(pCtx, &pPathname)  );

	SG_pathname__set__from_cwd(pCtx, pPathname);
	VERIFY_COND("pathname_cwd", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	// TODO we can't verify the value of cwd without getting it ourselves.
	// TODO so for now, we just dump it to the log for manual inspection.
	// TODO we should cd to some known place like /tmp or c:/temp and then
	// TODO test this.

	INFOP("pathname_cwd",("cwd is [%s]",SG_pathname__sz(pPathname)));

	SG_PATHNAME_NULLFREE(pCtx, pPathname);

	return 1;
}

int u0014_pathname__set_0(SG_context * pCtx, const char * szutf8path, const char * szExpected, SG_bool bTrimShouldSucceed)
{
	// test sg_pathname path normalization.

	SG_pathname * pPathname;
	int bExpectedHasFinalSlash;
	SG_bool bResultHasFinalSlash, bResultHadFinalSlash;
	SG_bool bCanRemoveFinalSlash;

	VERIFY_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC(pCtx, &pPathname)  );

	// set the pathname using the given absolute or relative path.
	// verify that we get a normalized absolute path as expected.

	SG_pathname__set__from_sz(pCtx,pPathname,szutf8path);
	VERIFYP_CTX_IS_OK("pathname_set_0", pCtx, ("norm[%s]",szutf8path));
	VERIFYP_COND("pathname_set_0",(strcmp(SG_pathname__sz(pPathname),szExpected) == 0),
			("norm[%s] yields[%s] expected[%s]",
			 szutf8path,SG_pathname__sz(pPathname),szExpected));
	SG_context__err_reset(pCtx);

	// see if the pathname has a final slash and if it matches our expectation.

	bExpectedHasFinalSlash = (szExpected[strlen(szExpected)-1] == '/');
	SG_pathname__has_final_slash(pCtx,pPathname,&bResultHasFinalSlash,&bCanRemoveFinalSlash);
	VERIFYP_CTX_IS_OK("pathname_set_0", pCtx, ("pPathname[%s] expected[%s] final_slash[%d,%d]",
			  SG_pathname__sz(pPathname),szExpected,bResultHasFinalSlash,bExpectedHasFinalSlash));
	VERIFYP_COND("pathname_set_0",(bResultHasFinalSlash == bExpectedHasFinalSlash),
			 ("pPathname[%s] expected[%s] final_slash[%d,%d]",
			  SG_pathname__sz(pPathname),szExpected,bResultHasFinalSlash,bExpectedHasFinalSlash));
	SG_context__err_reset(pCtx);

	if (bResultHasFinalSlash)
		VERIFYP_COND("pathname_set_0",(bCanRemoveFinalSlash == bTrimShouldSucceed),
				("pPathname[%s] has final slash [can Remove %d][trim should work %d]",
				 SG_pathname__sz(pPathname),bCanRemoveFinalSlash,bTrimShouldSucceed));

	// try to trim final slash (when present).  this will (legally) fail for
	// certain pathnames (such as the root directory).  and it will return OK
	// when we don't have a final slash.

	SG_pathname__remove_final_slash(pCtx,pPathname,&bResultHadFinalSlash);
	if (bTrimShouldSucceed)
		VERIFYP_CTX_IS_OK("pathname_set_0",pCtx, ("pPathname[%s] trimFinalSlash failed when success expected",SG_pathname__sz(pPathname)));
	else
		VERIFYP_CTX_HAS_ERR("pathname_set_0",pCtx, ("pPathname[%s] trimFinalSlash succeeded when failure expected",SG_pathname__sz(pPathname)));
	if (bExpectedHasFinalSlash && bTrimShouldSucceed)
	{
		VERIFYP_COND("pathname_set_0",(SG_pathname__length_in_bytes(pPathname)+1 == strlen(szExpected)),
				("pathname[%s] should be 1 byte shorter than[%s]",
				 SG_pathname__sz(pPathname),szExpected));
	}
	else
	{
		// no final slash or it shouldn't be trimmed (root)

		VERIFYP_COND("pathname_set_0",(strcmp(SG_pathname__sz(pPathname),szExpected) == 0),
				("after trim: pPathname[%s] expected[%s]", SG_pathname__sz(pPathname),szExpected));
	}
	SG_context__err_reset(pCtx);

	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	return 1;
}

#if defined(MAC) || defined(LINUX)
int u0014_pathname__set(SG_context * pCtx)
{
	// test absolute paths
	SG_pathname * pPathname = NULL;

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/def","/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/","/",SG_FALSE)  );

	//Commented out by jeremy, since we don't expand ~ anymore
	// test ~ paths (relative to us)

	setenv("HOME","/home/foo",1);

	VERIFY_ERR_CHECK_DISCARD(  SG_pathname__alloc__user_appdata_directory(pCtx, &pPathname)  );

	VERIFYP_COND("pathname_set",
				(strcmp(SG_pathname__sz(pPathname),"/home/foo/")==0),
				 ("pathname_set given[%s] expect[%s]",
						 SG_pathname__sz(pPathname),"/home/foo/"));
	/*
	 *Commented out by Jeremy.  We no longer do ~ expansion
	 VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"~/abc","/home/foo/abc",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"~","/home/foo/",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"~/","/home/foo/",SG_TRUE)  );
*/
	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	unsetenv("HOME");

	// test ~ paths where we have to use $USER to lookup the PW entry and
	// get the home directory from it, because $HOME is not set.
	// since we don't know which user is running the test, we set $USER to
	// "root" because that account is always present.
	//
	// BUT the default location of root's home directory varies by platform.
	// TODO we should really change this to call getpwent and use pw->pw_dir,
	// TODO but that is what the routine we're calling is doing.... so i'm
	// TODO not sure how useful that would be.  my purpose in testing is to
	// TODO make sure that the ~ gets handled properly.

#if defined(MAC)
#define ROOTHOME "/var/root"
#else
#define ROOTHOME "/root"
#endif

	setenv("USER","root",1);
	/*
		 *Commented out by Jeremy.  We no longer do ~ expansion
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"~/abc",  ROOTHOME "/abc",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"~",      ROOTHOME "/",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"~/",     ROOTHOME "/",SG_TRUE)  );
*/
	VERIFY_ERR_CHECK_DISCARD(  SG_pathname__alloc__user_appdata_directory(pCtx, &pPathname)  );
#if defined(MAC)
	VERIFYP_COND("pathname_set",
				(strcmp(SG_pathname__sz(pPathname),"/var/root/")==0),
				 ("pathname_set given[%s] expect[%s]",
						 SG_pathname__sz(pPathname),"/var/root/"));
#else
	VERIFYP_COND("pathname_set",
					(strcmp(SG_pathname__sz(pPathname),"/root/")==0),
					 ("pathname_set given[%s] expect[%s]",
							 SG_pathname__sz(pPathname),"/root/"));
#endif
	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	unsetenv("USER");

	// TODO test ~ paths where we have to do getpwuid(getuid())

	//////////////////////////////////////////////////////////////////

	// TODO test paths relative to cwd.
	// TODO we should cd to someplace like /tmp or C:/temp and then test this.

	//////////////////////////////////////////////////////////////////

	// test . collapsing

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/./def","/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/././def","/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/./././././def","/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/./abc/def","/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/./","/abc/",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/.","/abc/",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/.","/",SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/./","/",SG_FALSE)  );

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/.abc/./def","/.abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc././def","/abc./def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/./def.","/abc/def.",SG_TRUE)  );

	//////////////////////////////////////////////////////////////////

	// test .. collapsing

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/../def","/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/../def/../","/",SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/../def/..","/",SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/def/ghi/../..","/abc/",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"/abc/def/ghi/../../","/abc/",SG_TRUE)  );

	// TODO test invalid .. sequences.  (such as falling off left from too many ..'s)

	//////////////////////////////////////////////////////////////////

	return 1;
}
#endif
#if defined(WINDOWS)
int u0014_pathname__set(SG_context * pCtx)
{
	// test absolute paths

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/def","c:/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"x:/","x:/",SG_FALSE)  );

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:\\abc\\def","c:/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"x:\\","x:/",SG_FALSE)  );

	//////////////////////////////////////////////////////////////////

	// windows doesn't do ~ or ~user paths

	//////////////////////////////////////////////////////////////////

	// TODO test paths relative to cwd.
	// TODO we should cd to someplace like /tmp or C:/temp and then test this.

	//////////////////////////////////////////////////////////////////

	// test . collapsing

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/./def","c:/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/././def","c:/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/./././././def","c:/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/./abc/def","c:/abc/def",SG_TRUE)  );

	// WARNING: we get different results for "C:/abc/./" and "C:/abc/."

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/./","c:/abc/",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/.","c:/abc",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/.","c:/",SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/./","c:/",SG_FALSE)  );

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/.abc/./def","c:/.abc/def",SG_TRUE)  );

	// WARNING: the file or directory "foo." is equivalent to "foo".
	// WARNING: windows silently eats the '.' when there is no suffix.
	//u0014_pathname__set_0("c:/abc././def","c:/abc./def");
	//u0014_pathname__set_0("c:/abc/./def.","c:/abc/def.");

	//////////////////////////////////////////////////////////////////

	// test .. collapsing

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/../def","c:/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/../def/../","c:/",SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/../def/..","c:/",SG_FALSE)  );

	// WARNING: we get different results for these two.
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/def/ghi/../..","c:/abc",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"c:/abc/def/ghi/../../","c:/abc/",SG_TRUE)  );

	// TODO test invalid .. sequences.  (such as falling off left from too many ..'s)

	//////////////////////////////////////////////////////////////////

	// test UNC paths

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"//server/share/abc/def","//server/share/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"\\\\server\\share\\abc\\def","//server/share/abc/def",SG_TRUE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__set_0(pCtx,"//server/share/abc/../def","//server/share/def",SG_TRUE)  );

	return 1;
}
#endif

void u0014_pathname__append_0(SG_context * pCtx, SG_pathname * pPathname,const char * szRel,const char * szExpect)
{
	// clone initial value in pPathname because we destroy (and rebuild) it during append operations.
	SG_string * pStrInitialValue = NULL;

	SG_STRING__ALLOC__SZ(pCtx, &pStrInitialValue, SG_pathname__sz(pPathname));
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__append__from_sz(pCtx,pPathname,szRel);

	VERIFYP_CTX_IS_OK("pathname_append", pCtx, ("pathname_append_0 initial[%s] append[%s] expect[%s]",
			  SG_string__sz(pStrInitialValue),szRel,szExpect));

	VERIFYP_COND("pathname_append",
			(strcmp(SG_pathname__sz(pPathname),szExpect)==0),
			 ("pathname_append_0 initial[%s] append[%s] expect[%s]",
			  SG_string__sz(pStrInitialValue),szRel,szExpect));

	SG_context__err_reset(pCtx);

	SG_STRING_NULLFREE(pCtx, pStrInitialValue);
}

int u0014_pathname__append(SG_context * pCtx)
{
	// test pathname concatenation

#if defined(MAC) || defined(LINUX)
#define BASE "/abc"
#endif
#if defined(WINDOWS)
#define BASE "c:/abc"
#endif

	SG_pathname * pPathname;

	VERIFY_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC(pCtx, &pPathname)  );

	SG_pathname__set__from_sz(pCtx,pPathname,BASE);
	VERIFYP_CTX_IS_OK("pathname_append", pCtx, ("pathname_set given[%s] expect[%s]",BASE,BASE));
	VERIFYP_COND("pathname_append",(strcmp(SG_pathname__sz(pPathname),BASE)==0),("pathname_set given[%s] expect[%s]",BASE,BASE));
	SG_context__err_reset(pCtx);

	// append several components.

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__append_0(pCtx,pPathname,"def", BASE "/def")  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__append_0(pCtx,pPathname,"ghi", BASE "/def/ghi")  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__append_0(pCtx,pPathname,"jkl", BASE "/def/ghi/jkl")  );

	// try some trickery

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__append_0(pCtx,pPathname,"../../foo", BASE "/def/foo")  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__append_0(pCtx,pPathname,"./bar",     BASE "/def/foo/bar" )  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__append_0(pCtx,pPathname,"xyz",       BASE "/def/foo/bar/xyz" )  );

	// TODO try some stuff to make it break -- possibly valid relative paths, but
	// TODO ones that don't make any sense when appending to an existing absolute
	// TODO path -- ~user paths, c:foo, //host/share/x, etc.

	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	return 1;

#undef BASE
}

int u0014_pathname__trim(SG_context * pCtx)
{
	// test pathname trimming

#if defined(MAC) || defined(LINUX)
#define BASE "/"
#endif
#if defined(WINDOWS)
#define BASE "c:/"
#endif

	SG_pathname * pPathname;

	VERIFY_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC(pCtx, &pPathname)  );

	// try to trim the final component from a pathname without a final slash

	SG_pathname__set__from_sz(pCtx,pPathname,BASE "abc/def/ghi/jkl/mno");
	VERIFY_COND("pathname_trim",!SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_pathname__remove_last(pCtx,pPathname);
	VERIFYP_CTX_IS_OK("pathname_trim", pCtx, ("trim[%s]",SG_pathname__sz(pPathname)));
	VERIFYP_COND("pathname_trim",(strcmp(SG_pathname__sz(pPathname),BASE "abc/def/ghi/jkl/") == 0),
			("trim[%s] expected[%s]",SG_pathname__sz(pPathname),BASE "abc/def/ghi/jkl/"));
	SG_context__err_reset(pCtx);

	// try to trim the final component from a pathname with a final slash

	SG_pathname__remove_last(pCtx,pPathname);
	VERIFYP_CTX_IS_OK("pathname_trim", pCtx, ("trim[%s]",SG_pathname__sz(pPathname)));
	VERIFYP_COND("pathname_trim",(strcmp(SG_pathname__sz(pPathname),BASE "abc/def/ghi/") == 0),
			("trim[%s] expected[%s]",SG_pathname__sz(pPathname),BASE "abc/def/ghi/"));
	SG_context__err_reset(pCtx);

	// set pathname to a root path and verify that it complains.

	SG_pathname__set__from_sz(pCtx,pPathname,BASE);
	VERIFY_COND("pathname_trim",!SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_pathname__remove_last(pCtx,pPathname);
	VERIFYP_CTX_HAS_ERR("pathname_trim", pCtx, ("trim[%s]",SG_pathname__sz(pPathname)));
	SG_context__err_reset(pCtx);

#if defined(WINDOWS)
	// set pathname to a share root path and verify that it complains.

	SG_pathname__set__from_sz(pCtx,pPathname,"//host/share/abc");
	VERIFY_COND("pathname_trim",!SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	// remove "abc", this should succeed.

	SG_pathname__remove_last(pCtx,pPathname);
	VERIFYP_CTX_IS_OK("pathname_trim", pCtx, ("trim[%s]",SG_pathname__sz(pPathname)));
	SG_context__err_reset(pCtx);

	// now try bogus remove of "share", this should fail.

	SG_pathname__remove_last(pCtx,pPathname);
	VERIFYP_CTX_HAS_ERR("pathname_trim", pCtx, ("trim[%s]",SG_pathname__sz(pPathname)));
	SG_context__err_reset(pCtx);
#endif

	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	return 1;

#undef BASE
}

int u0014_pathname__unset(SG_context * pCtx)
{
	// test the IsSet bit.

	SG_pathname * pPathname = NULL;
	SG_pathname * pPathname1 = NULL;

	VERIFY_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC(pCtx,&pPathname)  );
	VERIFY_COND("pathname_unset",(pPathname != NULL));
	VERIFY_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC(pCtx,&pPathname1)  );
	VERIFY_COND("pathname_unset",(pPathname1 != NULL));

	// verify not initially set

	VERIFY_COND("pathname_unset", (SG_pathname__is_set(pPathname) == SG_FALSE));
	VERIFY_COND("pathname_unset", (SG_pathname__is_set(pPathname1) == SG_FALSE));

	// verify that all modifiers fail on an unset pathname

	SG_pathname__remove_last(pCtx, pPathname);
	VERIFY_CTX_ERR_EQUALS("pathname_unset", pCtx, SG_ERR_INVALIDARG);
	SG_context__err_reset(pCtx);

	SG_pathname__remove_final_slash(pCtx, pPathname, NULL);
	VERIFY_CTX_ERR_EQUALS("pathname_unset", pCtx, SG_ERR_INVALIDARG);
	SG_context__err_reset(pCtx);

	SG_pathname__has_final_slash(pCtx, pPathname, NULL, NULL);
	VERIFY_CTX_ERR_EQUALS("pathname_unset", pCtx, SG_ERR_INVALIDARG);
	SG_context__err_reset(pCtx);

	SG_pathname__append__from_sz(pCtx, pPathname, "foo");
	VERIFY_CTX_ERR_EQUALS("pathname_unset", pCtx, SG_ERR_INVALIDARG);
	SG_context__err_reset(pCtx);

	// verify we can't use unset pathname to set one.

	SG_pathname__set__from_pathname(pCtx, pPathname, pPathname1);
	VERIFY_CTX_ERR_EQUALS("pathname_unset", pCtx, SG_ERR_INVALIDARG);
	SG_context__err_reset(pCtx);

	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	SG_PATHNAME_NULLFREE(pCtx, pPathname1);

	return 1;
}

//////////////////////////////////////////////////////////////////

int u0014_pathname__dotdot_0(SG_context * pCtx,
							 const char * szBasePath,
							 const char * szDotDotPath,
							 const char * szExpectedPath,
							 SG_bool bExpectError)
{
	SG_pathname * pPathname;

	INFOP("dotdot",("Testing Base[%s] DotDot[%s] Expected[%s]",
					szBasePath,szDotDotPath,szExpectedPath));

	SG_PATHNAME__ALLOC__SZ(pCtx,&pPathname,szBasePath);
	VERIFY_CTX_IS_OK("dotdot", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__append__from_sz(pCtx,pPathname,szDotDotPath);
	if (bExpectError)
	{
		VERIFY_CTX_HAS_ERR("dotdot", pCtx);
		// if bogus .. sequence, our pathname should NOT have been modified.
		VERIFYP_COND("dotdot",
				(strcmp(SG_pathname__sz(pPathname),szBasePath)==0),
				("Pathname[%s] Expected[%s]",
				 SG_pathname__sz(pPathname),
				 szBasePath));
		SG_context__err_reset(pCtx);
	}
	else
	{
		VERIFY_CTX_IS_OK("dotdot", pCtx);
		// append of .. path successful, make sure result matches our expectation.
		VERIFYP_COND("dotdot",
				(strcmp(SG_pathname__sz(pPathname),szExpectedPath)==0),
				("Pathname[%s] Expected[%s]",
				 SG_pathname__sz(pPathname),
				 szExpectedPath));
		SG_context__err_reset(pCtx);
	}

	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	return 1;
}

int u0014_pathname__dotdot(SG_context * pCtx)
{
#if defined(MAC) || defined(LINUX)
  SG_UNUSED(pCtx);
	// TODO
	return 1;
#endif
#if defined(WINDOWS)

	// see warning in Windows version of _sg_pathname__normalize() about
	// GetFullPathNameW() and how/if it returns trailing slashes.
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__dotdot_0(pCtx, "c:/abc/def/",		"../",		"c:/abc/", SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__dotdot_0(pCtx, "c:/abc/def",		"../",		"c:/abc/", SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__dotdot_0(pCtx, "c:/abc/def/",		"..",		"c:/abc", SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__dotdot_0(pCtx, "c:/abc/def",		"..",		"c:/abc", SG_FALSE)  );

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__dotdot_0(pCtx, "c:/abc/def/",		"../../",	"c:/", SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__dotdot_0(pCtx, "c:/abc/def",		"../../",	"c:/", SG_FALSE)  );

	// see warning in Windows version of _sg_pathname__normalize() about
	// GetFullPathNameW() hiding some errors.
	//	u0014_pathname__dotdot_0("c:/abc/def/",		"../../../",	"xxx", SG_TRUE);
	//	u0014_pathname__dotdot_0("c:/abc/def",		"../../..",		"xxx", SG_TRUE);
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__dotdot_0(pCtx, "c:/abc/def/",		"../../../",	"c:/", SG_FALSE)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__dotdot_0(pCtx, "c:/abc/def",		"../../..",		"c:/", SG_FALSE)  );


	// abuse some long UNC paths.

	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__dotdot_0(pCtx,
							  ("c:/"
							  "Z_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "Y_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "X_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "W_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "V_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "U_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "T_123456789abcdef123456789abcdef123456789abcdef123456789abcdef"),
							 "../",
							 ("c:/"
							  "Z_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "Y_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "X_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "W_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "V_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"
							  "U_123456789abcdef123456789abcdef123456789abcdef123456789abcdef/"),
							 SG_FALSE)  );

	return 1;
#endif
}

int u0014_pathname__relative_normal(SG_context * pCtx)
{
	SG_pathname * p1 = NULL;
	SG_pathname * p2 = NULL;
	SG_string* pstr = NULL;

	SG_PATHNAME__ALLOC(pCtx, &p1);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__set__from_cwd(pCtx, p1);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_PATHNAME__ALLOC(pCtx, &p2);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__set__from_cwd(pCtx, p2);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__append__from_sz(pCtx, p2, "foo");
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__append__from_sz(pCtx, p2, "bar");
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__add_final_slash(pCtx, p2);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__make_relative(pCtx, p1, p2, &pstr);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	VERIFY_COND("rel", (0 == strcmp("foo/bar/", SG_string__sz(pstr))));

	SG_PATHNAME_NULLFREE(pCtx, p1);
	SG_PATHNAME_NULLFREE(pCtx, p2);
	SG_STRING_NULLFREE(pCtx, pstr);

	return 1;
}

int u0014_pathname__relative_fail(SG_context * pCtx)
{
	SG_pathname * p1 = NULL;
	SG_pathname * p2 = NULL;
	SG_string* pstr = NULL;

	SG_PATHNAME__ALLOC(pCtx, &p1);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__set__from_cwd(pCtx, p1);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__remove_final_slash(pCtx, p1, NULL);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_PATHNAME__ALLOC(pCtx, &p2);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__set__from_cwd(pCtx, p2);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__append__from_sz(pCtx, p2, "foo");
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__append__from_sz(pCtx, p2, "bar");
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__add_final_slash(pCtx, p2);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__make_relative(pCtx, p1, p2, &pstr);
	VERIFY_COND("should fail", (SG_context__has_err(pCtx)));
	SG_context__err_reset(pCtx);

	SG_PATHNAME_NULLFREE(pCtx, p1);
	SG_PATHNAME_NULLFREE(pCtx, p2);
	SG_STRING_NULLFREE(pCtx, pstr);

	return 1;
}

int u0014_pathname__relative_equal(SG_context * pCtx)
{
	SG_pathname * p1 = NULL;
	SG_pathname * p2 = NULL;
	SG_string* pstr = NULL;

	SG_PATHNAME__ALLOC(pCtx, &p1);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__set__from_cwd(pCtx, p1);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_PATHNAME__ALLOC(pCtx, &p2);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__set__from_cwd(pCtx, p2);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_pathname__make_relative(pCtx, p1, p2, &pstr);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	VERIFY_COND("null", (pstr == NULL));
	SG_context__err_reset(pCtx);

	SG_PATHNAME_NULLFREE(pCtx, p1);
	SG_PATHNAME_NULLFREE(pCtx, p2);
	SG_STRING_NULLFREE(pCtx, pstr);

	return 1;
}

int u0014_pathname__relative(SG_context * pCtx)
{
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__relative_normal(pCtx)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__relative_fail(pCtx)  );
	VERIFY_ERR_CHECK_DISCARD(  u0014_pathname__relative_equal(pCtx)  );

	return 1;
}

int u0014_pathname__get_last(SG_context * pCtx)
{
	SG_pathname* pp = NULL;
	SG_string* pstr = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pp, "/foo/bar/hello/world/")  );
	VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx, pp, &pstr)  );
	SG_PATHNAME_NULLFREE(pCtx, pp);
	VERIFY_COND("match", (0 == strcmp("world", SG_string__sz(pstr)))  );
	SG_STRING_NULLFREE(pCtx, pstr);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pp, "/foo/bar/hello/world")  );
	VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx, pp, &pstr)  );
	SG_PATHNAME_NULLFREE(pCtx, pp);
	VERIFY_COND("match", (0 == strcmp("world", SG_string__sz(pstr)))  );
	SG_STRING_NULLFREE(pCtx, pstr);

#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pp, "/")  );
	SG_pathname__get_last(pCtx, pp, &pstr);
	VERIFY_COND("should fail", (SG_context__has_err(pCtx)));
	SG_PATHNAME_NULLFREE(pCtx, pp);
#endif

#if defined(WINDOWS)
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pp, "c:/")  );
	SG_pathname__get_last(pCtx, pp, &pstr);
	VERIFY_COND("should fail", (SG_context__has_err(pCtx)));
	SG_PATHNAME_NULLFREE(pCtx, pp);
#endif

	SG_context__err_reset(pCtx);

	return 1;

fail:
	return -1;
}

//////////////////////////////////////////////////////////////////

TEST_MAIN(u0014_pathname)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0014_pathname__cwd(pCtx)  );
	BEGIN_TEST(  u0014_pathname__set(pCtx)  );
	BEGIN_TEST(  u0014_pathname__append(pCtx)  );
	BEGIN_TEST(  u0014_pathname__trim(pCtx)  );
	BEGIN_TEST(  u0014_pathname__unset(pCtx)  );
	BEGIN_TEST(  u0014_pathname__dotdot(pCtx)  );
	BEGIN_TEST(  u0014_pathname__relative(pCtx)  );
	BEGIN_TEST(  u0014_pathname__get_last(pCtx)  );

	TEMPLATE_MAIN_END;
}
