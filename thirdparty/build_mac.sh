#!/bin/sh
## Script to compile third-party packages that we use and that
## are not typically installed on the system or available in 
## MacPorts.
##
## This script will compile each package, "install" them into
## a "sgneeds" directory, and build a "sgneeds_<date>.zip" file.
## This ZIP file should be expanded at root; this will create:
##     /sgneeds/bin
##     /sgneeds/lib
##     /sgneeds/include
##     ....
## 
##################################################################
## Set the following variables to the name/version of the
## third-party source packages that you have:

SPIDERMONKEY_TARBALL=js-1.8.0-rc1.tar.gz

ICU_TARBALL=icu4c-4_4_1-src.tgz

##################################################################

ROOT=`pwd`
ARCHIVEROOT="${ROOT}"/archives
BUILDROOT="${ROOT}"/sgneeds
SRCDIR="${ROOT}"/src
BUILDROOT_INCDIR="${BUILDROOT}"/include
BUILDROOT_BINDIR="${BUILDROOT}"/bin
BUILDROOT_LIBDIR="${BUILDROOT}"/lib
rm -rf "${BUILDROOT}"
rm -rf "${SRCDIR}"
mkdir "${BUILDROOT}"
mkdir "${SRCDIR}"
mkdir "${BUILDROOT_INCDIR}"
mkdir "${BUILDROOT_INCDIR}"/sgbrings
mkdir "${BUILDROOT_BINDIR}"
mkdir "${BUILDROOT_LIBDIR}"

set -e

################################################################
## Build SpiderMonkey.
##
## There is a syntax error in JS 1.8.0-rc1 (jsprf.c:644) that
## GCC 4.2 (in XCode 3.2.3) doesn't like.  GCC 4.0 is fine with it.
## So fall back to the older compiler for building JS (CC=/usr/bin/gcc-4.0).

MY_JS_ROOT="${SRCDIR}"/js
MY_JS_LOG="${MY_JS_ROOT}"/_log_.txt
echo Unpacking JS
cd "${SRCDIR}"
tar xzf "${ARCHIVEROOT}"/${SPIDERMONKEY_TARBALL}
echo Building JS
cd "${MY_JS_ROOT}"/src
make CC=/usr/bin/gcc-4.0 BUILD_OPT=1 -f Makefile.ref
cd "${MY_JS_ROOT}"/src
cp "${MY_JS_ROOT}"/src/Darwin_OPT.OBJ/libjs.a "${BUILDROOT_LIBDIR}"/libsgbrings_js.a
cp "${MY_JS_ROOT}"/src/Darwin_OPT.OBJ/js "${BUILDROOT_BINDIR}"
mkdir "${BUILDROOT_INCDIR}"/sgbrings/js
cp "${MY_JS_ROOT}"/src/Darwin_OPT.OBJ/*.h "${BUILDROOT_INCDIR}"/sgbrings/js
cp "${MY_JS_ROOT}"/src/*.h "${BUILDROOT_INCDIR}"/sgbrings/js
cp "${MY_JS_ROOT}"/src/*.msg "${BUILDROOT_INCDIR}"/sgbrings/js
cp "${MY_JS_ROOT}"/src/*.tbl "${BUILDROOT_INCDIR}"/sgbrings/js

################################################################
## Build ICU.
##
## Because sglib is built with -m32, we need to use --with-library-bits=32.

MY_ICU_ROOT="${SRCDIR}"/icu
MY_ICU_LOG="${MY_ICU_ROOT}"/_log_.txt
echo Unpacking ICU
cd "${SRCDIR}"
tar xzf "${ARCHIVEROOT}"/${ICU_TARBALL}
echo Building ICU
cd "${MY_ICU_ROOT}"/source
chmod +x runConfigureICU configure install-sh
./runConfigureICU MacOSX --prefix="${BUILDROOT}" --disable-icuio --disable-layout --with-library-bits=32 --with-iostream=none > "${MY_ICU_LOG}" 2>&1
cd "${MY_ICU_ROOT}"/source
make >> "${MY_ICU_LOG}" 2>&1
cd "${MY_ICU_ROOT}"/source
make install >> "${MY_ICU_LOG}" 2>&1

################################################################
cd "${ROOT}"
echo Building sgneeds package
MY_ZIP=sgneeds_`date "+%Y_%m_%d_%H%M"`.zip
zip -r ${MY_ZIP} sgneeds
echo Built libraries are in ${MY_ZIP}

