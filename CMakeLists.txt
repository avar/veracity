
project(sprawl C)

cmake_minimum_required(VERSION 2.6)

IF (NOT CMAKE_BUILD_TYPE)
SET(CMAKE_BUILD_TYPE "Debug" CACHE STRING  "build type determining compiler flags" FORCE )
ENDIF (NOT CMAKE_BUILD_TYPE)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

set(SG_OS_LIBS "")

IF (NOT BUILDNUMBER)
	SET (BUILDNUMBER "0" CACHE STRING "Build number for this build version - !!QUOTED!!")
ENDIF (NOT BUILDNUMBER)
IF (BUILDNUMBER)
	add_definitions(-DBUILDNUMBER=${BUILDNUMBER})
ENDIF (BUILDNUMBER)

IF ("$ENV{SPRAWL_BUILDER}" STREQUAL "1")
	add_definitions(-DSG_REAL_BUILD)
	OPTION(SG_NIGHTLY_BUILD "When enabled, slower nightly build tasks are performed." OFF)
	if (SG_NIGHTLY_BUILD)
		ADD_DEFINITIONS(-DSG_NIGHTLY_BUILD)
	endif()
	OPTION(SG_LONGTESTS "When enabled, really slow occasional build tasks are performed." OFF)
	if (SG_LONGTESTS)
		ADD_DEFINITIONS(-DSG_LONGTESTS)
	endif()
ENDIF()

# ----------------------------------------------------------------
if(CMAKE_COMPILER_IS_GNUCC)
# TODO should we add -Wconversion to make this more like the MSFT compiler?
# TODO or -Wsign-conversion ?

# TODO would -Wunreachable-code make this more like the MSFT compiler?

# TODO how about -Wmissing-prototypes ??  this would make the compiler VERY
# TODO fussy, but it might catch some interesting problems

# TODO I would love to add 
# TODO -Wdisallowed-function-list=strcpy,sprintf,etc
# TODO but gcc on the Mac is at version 4.0 and this
# TODO option was added in some later version.

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wdeclaration-after-statement -Wstrict-prototypes")

# the apple_unicode code violates the strict-aliasing rules
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")

OPTION(SG_GCOV "Compile everything with -fprofile-arcs -ftest-coverage for gcov" OFF)

if(SG_GCOV)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

OPTION(SG_GPROF "Compile everything with -pg for gprof" OFF)

if(SG_GPROF)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
endif()

endif() #CMAKE_COMPILER_IS_GNUCC

# ----------------------------------------------------------------
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")

set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} /sgneeds/include)
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /sgneeds/lib) 

add_definitions(-DMAC)

add_definitions(-DSG_BUILD_FLAG_FEATURE_XATTR)
add_definitions(-DSG_BUILD_FLAG_TEST_XATTR)
# the following two lines are needed for spidermonkey
add_definitions(-DXP_UNIX)
#add_definitions(-DDARWIN)

OPTION(SG_SATURN "Compile everything for use with the Saturn profiler on Intel Macs" OFF)

if(SG_SATURN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -finstrument-functions")
####    set(SG_OS_LIBS "libSaturn.dylib")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lsaturn")
endif()

# The -m32 flag is needed on Snow Leopard
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")

endif () # Darwin

# ----------------------------------------------------------------
IF(WIN32 AND NOT CMAKE_COMPILER_IS_GNUCC)

set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} c:/sgneeds/include)
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} c:/sgneeds/lib) 

IF (NOT VC_MP)
SET (VC_MP 4 CACHE STRING "VC /MP option: number of modules compiled in parallel in Visual Studio")
ENDIF (NOT VC_MP)


add_definitions(-DWINDOWS)
add_definitions(-DXP_WIN)
add_definitions(-DEXPORT_JS_API)

SET(CMAKE_C_WARNING_LEVEL 4)
IF(CMAKE_C_FLAGS MATCHES "/W[0-4]")
  STRING(REGEX REPLACE "/W[0-4]" "/W4"
         CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
ELSE(CMAKE_C_FLAGS MATCHES "/W[0-4]")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
ENDIF(CMAKE_C_FLAGS MATCHES "/W[0-4]")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -WX -DUNICODE -MP${VC_MP}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Zi -MDd")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MD")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:msvcrt.lib ")
	SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:msvcrt.lib ")
endif() # (CMAKE_BUILD_TYPE STREQUAL "Debug")

set(SG_OS_LIBS rpcrt4.lib winmm.lib)

endif () # WIN32 AND NOT CMAKE_COMPILER_IS_GNUCC

# ----------------------------------------------------------------
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")

add_definitions(-DLINUX)
add_definitions(-DXP_UNIX)

add_definitions(-DSG_BUILD_FLAG_FEATURE_XATTR)
##### TODO See note in sg_attributes.c about SE-Linux.
##### TODO add_definitions(-DSG_BUILD_FLAG_TEST_XATTR)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

# This link describes all of the various feature flags such as largefiles
# and posix vs gnu vs bsd.
#     http://www.gnu.org/s/libc/manual/html_node/Feature-Test-Macros.html
#
# I added _GNU_SOURCE go get declarations for qsort_r() on Linux.
#
# _GNU_SOURCE causes the definition of strerror_r() to change, see sg_error.c
#
add_definitions(-D_GNU_SOURCE)

# TODO this should probably call getconf LFS_CFLAGS instead
#
# TODO _GNU_SOURCE also turns on the LFS stuff, so we may not 
# TODO need to explicitly set them below.
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64) 

set(SG_OS_LIBS uuid pthread dl m)

endif () # "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux"

# ----------------------------------------------------------------
# Now the sgneeds stuff
# ----------------------------------------------------------------

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")

find_package(Sqlite3 REQUIRED)
INCLUDE_DIRECTORIES(${SQLITE3_INCLUDE_DIR})
set(SG_THIRDPARTY_LIBRARIES ${SG_THIRDPARTY_LIBRARIES} ${SQLITE3_LIBRARIES})

find_package(ZLIB REQUIRED)
INCLUDE_DIRECTORIES(${zlib_INCLUDE_DIR})
set(SG_THIRDPARTY_LIBRARIES ${SG_THIRDPARTY_LIBRARIES} ${ZLIB_LIBRARIES})

#### find_package(Expat REQUIRED)
#### INCLUDE_DIRECTORIES(${EXPAT_INCLUDE_DIR})
#### set(SG_THIRDPARTY_LIBRARIES ${SG_THIRDPARTY_LIBRARIES} ${EXPAT_LIBRARIES})

find_package(ICU REQUIRED)
INCLUDE_DIRECTORIES(${ICU_INCLUDE_DIR})
set(SG_THIRDPARTY_LIBRARIES ${SG_THIRDPARTY_LIBRARIES} ${ICU_LIBRARIES})

#### find_package(OpenSSL REQUIRED)
#### INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
#### set(SG_THIRDPARTY_LIBRARIES ${SG_THIRDPARTY_LIBRARIES} ${OPENSSL_LIBRARIES})

find_package(CURL REQUIRED)
INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
set(SG_THIRDPARTY_LIBRARIES ${SG_THIRDPARTY_LIBRARIES} ${CURL_LIBRARIES})

# ----------------------------------------------------------------
# And the sgbrings stuff
# ----------------------------------------------------------------

FIND_PATH(SGBRINGS_JS_INCDIR NAMES sgbrings/js/jsapi.h)
MARK_AS_ADVANCED(SGBRINGS_JS_INCDIR)
INCLUDE_DIRECTORIES(${SGBRINGS_JS_INCDIR})
FIND_LIBRARY(SGBRINGS_JS_LIB NAMES sgbrings_js )
set(SG_THIRDPARTY_LIBRARIES ${SG_THIRDPARTY_LIBRARIES} ${SGBRINGS_JS_LIB})

# ----------------------------------------------------------------
include_directories(src/libraries/include)
include_directories(src/libraries/sghash)

# ----------------------------------------------------------------

enable_testing()
add_subdirectory(src)
add_subdirectory(testsuite)

# TODO the cmake wiki says we'll need this snippet.  Also, it
# TODO shows an interesting use of set_target_properties
# if(WIN32)
# set_target_properties(WindowApplicationExample PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
# set_target_properties(WindowApplicationExample PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
# set_target_properties(WindowApplicationExample PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
# set_target_properties(WindowApplicationExample PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
# set_target_properties(WindowApplicationExample PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
# set_target_properties(WindowApplicationExample PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
# endif(WIN32)

if(SG_GCOV)
    add_custom_target(
        root_coverage_prep
        COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/html
        COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/lcov-html
        COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/zcov-html
        COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/_lcov_.info
        COMMAND lcov --directory ${CMAKE_CURRENT_BINARY_DIR} --zerocounters
        COMMENT "Prepping for root code coverage"
        )

    add_custom_target(
        root_coverage_lcov
        COMMAND lcov --directory ${CMAKE_CURRENT_BINARY_DIR} --capture --output-file _lcov_.info
        COMMAND genhtml --output-directory ${CMAKE_CURRENT_BINARY_DIR}/lcov-html _lcov_.info
        COMMAND echo "Results are in ${CMAKE_CURRENT_BINARY_DIR}/lcov-html/index.html"
        COMMENT "Publishing gcov/lcov code coverage"
        )

    add_custom_target(
        root_coverage_zcov
        COMMAND zcov-scan "./output.zcov" .
        COMMAND zcov-genhtml --root="${CMAKE_SOURCE_DIR}" "./output.zcov" "./zcov-html"
        COMMAND echo "Results are in ${CMAKE_CURRENT_BINARY_DIR}/zcov-html/index.html"
        COMMENT "Publishing zcov code coverage"
        )

    add_custom_target(
        root_showvars
        COMMAND echo "CMAKE_CURRENT_BINARY_DIR = ${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND echo "SGLIB_OBJECT_DIR = ${SGLIB_OBJECT_DIR}"
        COMMAND echo "CMAKE_FILES_DIRECTORY = ${CMAKE_FILES_DIRECTORY}"
        COMMAND echo "CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND echo "CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}"
        COMMENT "SHOWVARS"
	)
endif()

