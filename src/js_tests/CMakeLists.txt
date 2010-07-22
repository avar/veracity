## Add "test" target to run the javascript tests.
## This causes them to be run whenever you type "make test".
## If you are in this directory, just the javascript tests will run.
## If you are at top level, then both the JS and C tests will run.
##
## WARNING: We go through a few hurdles so that the tests can reference the SOURCE
## WARNING: directory for the test scripts and the BUILD directory for the .EXEs
## WARNING: and temp space to put the generated REPOs, Working Directories, and
## WARNING: other trash.  The goal here is to NOT dirty up the SOURCE directory.
##
## TODO It'd be nice if we could change vscript and/or vscript_tests.js to take a pathname
## TODO to the SOURCE directory.  (And maybe add the vv and vscript exe directories
## TODO to our PATH.

## Use GLOB to get a list of all of the st*.js test files.  These
## come in as full pathnames rather than entrynames it seems.

FILE(GLOB SG_JS_TESTS st*.js)


IF (WIN32)

	## The .BAT file needs to be given pathnames with DOS-style backslashes.
	## The following command works, in that it converts the slashes and the
	## pathname gets into the .BAT file properly, but it causes problems for
	## CTest somewhere and we get a "Policy CMP0010" warning because some of
	## the \s in z:\shared\v\veracity... isn't a valid escape sequence.
	##
	## FILE(TO_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR} DOS_CURRENT_SOURCE_DIR)
	##
	## So, we do it the hard way.

	MACRO(MAKE_WINDOWS_PATH pathname)
		## Convert '/' to "\\" (doubled up to get past parser here)
		STRING(REPLACE "/" "\\\\" ${pathname} "${${pathname}}")
	ENDMACRO(MAKE_WINDOWS_PATH)
	SET(DOS_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
	MAKE_WINDOWS_PATH(DOS_CURRENT_SOURCE_DIR)		

	## The .BAT file runs vscript.exe and some of the JS tests run vv.exe,
	## so we need to be sure that they are in our path.  The NMAKE version
	## puts them in the cmd & script directory; the MSVC .sln/.vcproj version
	## puts them in cmd/Debug and script/Debug.  For Release builds this is
	## cmd/Release and script/Release.
	##
	## So we need to "fix" things here; we set the relative portion of the path.

	IF(CMAKE_MAKE_PROGRAM MATCHES "nmake")
		SET(MY_RELEXE_DIR ".")
	ELSE()
		SET(MY_RELEXE_DIR ${CMAKE_BUILD_TYPE})
	ENDIF()

	foreach(js_pathname ${SG_JS_TESTS})
		GET_FILENAME_COMPONENT(js_filename ${js_pathname} NAME)
		add_test(${js_filename} ${CMAKE_CURRENT_SOURCE_DIR}/js_test.bat ${DOS_CURRENT_SOURCE_DIR} ${js_filename} ${MY_RELEXE_DIR})
	endforeach(js_pathname ${SG_JS_TESTS})

ELSE()
	# Otherwise, we have a unix-based system with BASH.

	foreach(js_pathname ${SG_JS_TESTS})
		GET_FILENAME_COMPONENT(js_filename ${js_pathname} NAME)
		add_test(${js_filename} /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/js_test.sh ${CMAKE_CURRENT_SOURCE_DIR} ${js_filename})
	endforeach(js_pathname ${SG_JS_TESTS})

ENDIF()
