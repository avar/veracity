#!/bin/sh
################################################################
##
## by default we build sgbrings stuff directly
## in the source tree, rather than in a cmake
## generated build tree.
##
## if you want to change that, set ARCHIVEROOT
## before you start this script to point to the
## .../sprawl/thirdparty/archives directory in
## your source tree ***AND*** cd to your build
## directory and run the script using a full
## pathname.
##
## for example:
##     cd /build/sprawl/thirdparty
##     ARCHIVEROOT=/shared/sprawl/thirdparty/archives /shared/sprawl/thirdparty/build_linux.sh
##     
################################################################
##
## we install the sgbrings stuff directly into
## /usr/local so you'll need to be root.
##
################################################################

ROOT=`pwd`
ARCHIVEROOT="${ARCHIVEROOT:-${ROOT}/archives}"
SRCDIR="${ROOT}"/src
DEST=/usr/local

####set -v
####set -x

echo Removing leftovers from previous builds
rm -rf "${SRCDIR}"
echo Creating directories
mkdir "${SRCDIR}"

# Stop on any error
set -e

################################################################
## js

MY_JS_ROOT="${SRCDIR}"/js
MY_JS_LOG="${MY_JS_ROOT}"/_log_.txt
echo Unpacking JS
cd "${SRCDIR}"
tar xzf "${ARCHIVEROOT}"/js-1.8.0-rc1.tar.gz
cp "${ARCHIVEROOT}"/js_win32.mak "${MY_JS_ROOT}"/src
echo Building JS
cd "${MY_JS_ROOT}"/src
make BUILD_OPT=1 -f Makefile.ref
cd "${MY_JS_ROOT}"/src
cp "${MY_JS_ROOT}"/src/Linux_All_OPT.OBJ/libjs.a ${DEST}/lib/libsgbrings_js.a
mkdir ${DEST}/include/sgbrings
mkdir ${DEST}/include/sgbrings/js
cp "${MY_JS_ROOT}"/src/Linux_All_OPT.OBJ/*.h ${DEST}/include/sgbrings/js
cp "${MY_JS_ROOT}"/src/*.h ${DEST}/include/sgbrings/js
cp "${MY_JS_ROOT}"/src/*.msg ${DEST}/include/sgbrings/js
cp "${MY_JS_ROOT}"/src/*.tbl ${DEST}/include/sgbrings/js

