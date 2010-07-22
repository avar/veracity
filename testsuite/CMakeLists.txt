
set(SG_TESTS
u0001_stdint.c
u0002_id.c
u0003_errno.c
u0004_file.c
u0005_string.c
u0007_hex.c
u0008_misc_utils.c
u0010_dbrecord.c
u0012_utf8.c
u0013_fsobj.c
u0014_pathname.c
u0015_dbcriteria.c
u0015_dir.c
u0019_xmlwriter.c
u0020_utf8pathnames.c
u0023_vcdiff.c
u0024_repo_query_implementation.c
u0025_jsonwriter.c
u0026_jsonparser.c
u0027_pool.c
u0028_vhash.c
u0029_bitvec256.c
u0031_repopath.c
u0032_repo.c
u0034_repo_treenode.c
u0036_committing.c
u0037_dag.c
u0038_closet.c
u0040_unc.c
u0041_rbtree.c
u0042_portability.c
u0043_pendingtree.c
u0044_xattr.c
u0045_console.c
u0046_pendport.c
u0047_repo_blobs.c
u0048_multidag.c
u0050_logstuff.c
u0051_hidlookup.c
u0052_zip.c
u0054_repo_encodings.c
u0055_vector.c
u0056_exec.c
u0057_file_locks.c
u0058_repotx.c
u0059_js.c
u0061_push.c
u0062_str_utils.c
u0063_error_context.c
u0065_vector_i64.c
u0066_logging.c
u0068_filespec.c
u0069_treendx.c
u0070_treenode_search.c
u0072_repo_hash_method.c
u0073_pull.c
u0074_web_utils.c
u0075_pendingtree_log.c
u0076_time.c
u0077_jsondb.c
u0078_diff.c
u0079_mutex.c
u0081_varray.c
u0104_treenode_entry.c
u0105_repopath.c
u1000_repo_script.c

)

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

	## The .BAT file runs the test, and some of the tests run vv.exe,
	## so we need to be sure that it is in our path.  The NMAKE version
	## puts vv in the cmd directory; the MSVC .sln/.vcproj version puts
	## vv in cmd/Debug.  For Release builds this is cmd/Release.
	##
	## So we need to "fix" things here; we set the relative portion of the path.

	IF(CMAKE_MAKE_PROGRAM MATCHES "nmake")
		SET(MY_RELEXE_DIR ".")
	ELSE()
		SET(MY_RELEXE_DIR ${CMAKE_BUILD_TYPE})
	ENDIF()

	MACRO(ADD_C_TEST testname)
		add_test(${testname} ${CMAKE_CURRENT_SOURCE_DIR}/c_test.bat ${testname} ${DOS_CURRENT_SOURCE_DIR} ${MY_RELEXE_DIR})
	ENDMACRO(ADD_C_TEST)

ELSE()
	# Otherwise, we have a unix-based system with BASH.

	MACRO(ADD_C_TEST testname)
		add_test(${testname} /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/c_test.sh ${testname} ${CMAKE_CURRENT_SOURCE_DIR})
	ENDMACRO(ADD_C_TEST)

ENDIF()


# Add an executable and test for each test
foreach(ut ${SG_TESTS})
string(REPLACE ".c" "" basename ${ut})
add_executable(${basename} ${ut})
target_link_libraries(${basename} sglib ${SG_THIRDPARTY_LIBS} ${SG_OS_LIBS})
add_dependencies(${basename} template_check)
add_test(${basename} ${EXECUTABLE_OUTPUT_PATH}/${basename} ${CMAKE_CURRENT_SOURCE_DIR})
endforeach(ut ${SG_TESTS})

# special case for u0064_mrg, which needs PATH set for vv
add_executable(u0064_mrg u0064_mrg.c)
target_link_libraries(u0064_mrg sglib ${SG_THIRDPARTY_LIBS} ${SG_OS_LIBS})
## add_test(u0064_mrg ${EXECUTABLE_OUTPUT_PATH}/u0064_mrg ${CMAKE_CURRENT_SOURCE_DIR})
ADD_C_TEST(u0064_mrg)


# special case for u1100_large_blob so we can pass it the large blob directory as an argument

add_executable(u1100_large_blob u1100_large_blob.c)
target_link_libraries(u1100_large_blob sglib ${SG_THIRDPARTY_LIBS} ${SG_OS_LIBS})
if (SG_LONGTESTS)
	add_test(u1100_large_blob ${EXECUTABLE_OUTPUT_PATH}/u1100_large_blob ${LARGE_BLOB_DIR})
endif()

# Generate u0000.  This test is another special case.
# It is generated at configure time by cmake here.
# It does not include unittests.h.  It simply calls
# every free function with NULL and returns 0 (success)
# at the end.  It is considered a successful test
# run if it doesn't crash.

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/u0000_free_null.c "#include <sg.h>\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_free_null.c "\nint main(void)\n{\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_free_null.c "\tSG_context * pCtx = NULL;\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_free_null.c "\tSG_context__alloc(&pCtx);\n\n")
file(GLOB hdrs ${CMAKE_SOURCE_DIR}/src/libraries/include/*.h) 
set(mall "")
foreach(h ${hdrs})
    file(READ ${h} f)
    string(REGEX MATCHALL "SG[a-z0-9_]+__free" mlist ${f})
    list(APPEND mall ${mlist})
endforeach()
list(REMOVE_DUPLICATES mall)
list(SORT mall)
foreach(m ${mall})
    if(m STREQUAL "SG_context__free")
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_free_null.c "\t${m}(NULL);\n")
    else()
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_free_null.c "\t${m}(pCtx,NULL);\n")
    endif()
endforeach()
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_free_null.c "\n\tSG_context__free(pCtx);\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_free_null.c "\treturn 0;\n}\n")
ADD_EXECUTABLE(u0000_free_null ${CMAKE_CURRENT_BINARY_DIR}/u0000_free_null.c)
target_link_libraries(u0000_free_null sglib ${SG_THIRDPARTY_LIBS} ${SG_OS_LIBS})
add_test(u0000_free_null u0000_free_null)

# Generate u0000_error_messages.. Another special case.

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "#include <sg.h>\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "#include \"${CMAKE_SOURCE_DIR}/testsuite/unittests.h\"\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "TEST_MAIN(u0000_error_messages)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "{\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\tchar sz[SG_ERROR_BUFFER_SIZE];\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\tint len;\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\tTEMPLATE_MAIN_START;\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\n")
set(mall "")
file(READ ${CMAKE_SOURCE_DIR}/src/libraries/include/sg_error_typedefs.h f)
string(REGEX MATCHALL "SG_ERR_SG_LIBRARY\\([1-9][0-9]*\\)" mlist ${f})
list(APPEND mall ${mlist})
foreach(m ${mall})
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\tSG_error__get_message(${m},sz,SG_ERROR_BUFFER_SIZE);\n")
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\tlen = strlen(sz);\n")
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\tVERIFY_COND(\"Descriptive error message should be defined for error code.\", len>20);\n")
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\tVERIFY_COND(\"Error message should not end with a period or a colon.\", sz[len-1]!='.' && sz[len-1]!=':');\n")
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\n")
endforeach()
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "\tTEMPLATE_MAIN_END;\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c "}\n")
ADD_EXECUTABLE(u0000_error_messages ${CMAKE_CURRENT_BINARY_DIR}/u0000_error_messages.c)
target_link_libraries(u0000_error_messages sglib ${SG_THIRDPARTY_LIBS} ${SG_OS_LIBS})
add_test(u0000_error_messages u0000_error_messages)

get_target_property(SGLIB_LOCATION sglib LOCATION)
get_filename_component(SGLIB_DIR ${SGLIB_LOCATION} PATH)
set(SGLIB_OBJECT_DIR "${SGLIB_DIR}${CMAKE_FILES_DIRECTORY}/sglib.dir")
#message(${SGLIB_OBJECT_DIR})
#message(${CMAKE_CURRENT_BINARY_DIR})

