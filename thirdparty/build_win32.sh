#!/bin/sh
ROOT=`pwd`
ARCHIVEROOT="${ROOT}"/archives
SGNEEDS_ROOT="${ROOT}"/sgneeds
SRCDIR="${ROOT}"/src
SGNEEDS_INCLUDE="${SGNEEDS_ROOT}"/include
SGNEEDS_BIN="${SGNEEDS_ROOT}"/bin
SGNEEDS_LIB="${SGNEEDS_ROOT}"/lib

echo Removing leftovers from previous builds
rm -rf "${SGNEEDS_ROOT}"
rm -rf "${SRCDIR}"
echo Creating directories
mkdir "${SGNEEDS_ROOT}"
mkdir "${SRCDIR}"
mkdir "${SGNEEDS_INCLUDE}"
mkdir "${SGNEEDS_BIN}"
mkdir "${SGNEEDS_LIB}"
mkdir "${SGNEEDS_INCLUDE}"/sgbrings

# Stop on any error
set -e

which nmake

#copy the MS C Runtime into sgneeds
#cp "$(which nmake | sed -e 's/nmake//')"/../redist/x86/Microsoft.*.CRT/*.dll "${SGNEEDS_BIN}"

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

echo 'call vcvars32.bat' > _compile_.bat
echo 'nmake -f js_win32.mak VC=vc9' >> _compile_.bat
chmod 755 _compile_.bat
./_compile_.bat >> "${MY_JS_LOG}" 2>&1

#nmake -f js_win32.mak >> "${MY_JS_LOG}" 2>&1
cd "${MY_JS_ROOT}"/src
cp "${MY_JS_ROOT}"/src/sgbrings_js.lib "${SGNEEDS_LIB}"/
mkdir "${SGNEEDS_INCLUDE}"/sgbrings/js
cp "${MY_JS_ROOT}"/src/*.h "${SGNEEDS_INCLUDE}"/sgbrings/js
cp "${MY_JS_ROOT}"/src/*.msg "${SGNEEDS_INCLUDE}"/sgbrings/js
cp "${MY_JS_ROOT}"/src/*.tbl "${SGNEEDS_INCLUDE}"/sgbrings/js


##################################################################
## sqlite

MY_SQLITE_VERSION=3_6_23_1

MY_SQLITE_ROOT="${SRCDIR}"/sqlite-${MY_SQLITE_VERSION}
MY_SQLITE_LOG="${SRCDIR}"/sqlite-${MY_SQLITE_VERSION}/_log_.txt

echo
echo Unpacking SQLITE
# sqlite.zip does not contain top-level directory (like the .tar.gz does}.
# the zip file just contains the *.[ch] files.
mkdir "${MY_SQLITE_ROOT}"
cd "${MY_SQLITE_ROOT}"
unzip -q "${ARCHIVEROOT}"/sqlite-amalgamation-${MY_SQLITE_VERSION}.zip
echo Building SQLITE.
cd "${MY_SQLITE_ROOT}"

echo 'call vcvars32.bat' > _compile_.bat
echo 'cl /c -D_CRT_SECURE_NO_DEPRECATE /MD sqlite3.c' >> _compile_.bat
echo 'lib /out:sqlite3.lib sqlite3.obj' >> _compile_.bat
chmod 755 _compile_.bat
./_compile_.bat >> "${MY_SQLITE_LOG}" 2>&1

#cl /c -D_CRT_SECURE_NO_DEPRECATE /MD sqlite3.c >> "${MY_SQLITE_LOG}" 2>&1
#lib /out:sqlite3.lib sqlite3.obj >> "${MY_SQLITE_LOG}" 2>&1
echo Copying build outputs for SQLITE
cd "${MY_SQLITE_ROOT}"
cp sqlite3.lib "${SGNEEDS_LIB}"
cp sqlite3.h sqlite3ext.h "${SGNEEDS_INCLUDE}"

##################################################################
## zlib

MY_ZLIB_VERSION=1.2.5

MY_ZLIB_ROOT="${SRCDIR}"/zlib-${MY_ZLIB_VERSION}
MY_ZLIB_LOG="${SRCDIR}"/zlib-${MY_ZLIB_VERSION}/_log_.txt

echo
echo Unpacking ZLIB
cd "${SRCDIR}"
tar -xzf "${ARCHIVEROOT}"/zlib-${MY_ZLIB_VERSION}.tar.gz

echo Building ZLIB.
cd "${MY_ZLIB_ROOT}"
rm example.c
rm minigzip.c

echo 'call vcvars32.bat' > _compile_.bat
echo 'cl /c -D_CRT_SECURE_NO_DEPRECATE /MD *.c' >> _compile_.bat
echo 'lib /out:zlib.lib *.obj' >> _compile_.bat
chmod 755 _compile_.bat
./_compile_.bat >> "${MY_SQLITE_LOG}" 2>&1

echo Copying build outputs for ZLIB
cd "${MY_ZLIB_ROOT}"
cp zlib.lib "${SGNEEDS_LIB}"
cp zlib.h zconf.h "${SGNEEDS_INCLUDE}"


##################################################################
## libcurl

MY_LIBCURL_VERSION=7.21.0

MY_LIBCURL_ROOT="${SRCDIR}"/curl-${MY_LIBCURL_VERSION}
MY_LIBCURL_LOG="${SRCDIR}"/curl-${MY_LIBCURL_VERSION}/_log_.txt

echo
echo Unpacking LIBCURL
cd "${SRCDIR}"
tar -xzf "${ARCHIVEROOT}"/curl-${MY_LIBCURL_VERSION}.tar.gz

echo Building LIBCURL
cd "${MY_LIBCURL_ROOT}"

echo 'call vcvars32.bat' > _compile_.bat
echo 'nmake vc-dll VC=vc9' >> _compile_.bat
chmod 755 _compile_.bat
./_compile_.bat >> "${MY_LIBCURL_LOG}" 2>&1

echo Copying build outputs for LIBCURL
cp "${MY_LIBCURL_ROOT}"/lib/*.lib "${SGNEEDS_LIB}"
cp "${MY_LIBCURL_ROOT}"/lib/*.dll "${SGNEEDS_BIN}"
#cp "${MY_LIBCURL_ROOT}"/src/curl.exe "${SGNEEDS_BIN}"
cp -r "${MY_LIBCURL_ROOT}"/include/curl "${SGNEEDS_INCLUDE}"


################################################################
MY_ICU_ROOT="${SRCDIR}"/icu
MY_ICU_LOG="${MY_ICU_ROOT}"/_log_.txt
echo
echo Unpacking ICU
cd "${SRCDIR}"
tar xzf "${ARCHIVEROOT}"/icu4c-4_4_1-src.tgz
echo Building ICU
cd "${MY_ICU_ROOT}"/source/allinone
unset MAKEFLAGS
unset MAKELEVEL
unset MAKEOVERRIDES

echo 'call vcvars32.bat'                             > _compile_.bat
echo 'devenv allinone.sln /Upgrade'                 >> _compile_.bat
echo 'devenv allinone.sln /rebuild "Release|Win32"' >> _compile_.bat
chmod 755 _compile_.bat
./_compile_.bat >> "${MY_ICU_LOG}" 2>&1


mkdir -p "${SGNEEDS_INCLUDE}"/unicode
cd "${MY_ICU_ROOT}"
echo Copying build outputs for ICU

cp bin/icudt44.dll "${SGNEEDS_BIN}"
cp bin/icuuc44.dll "${SGNEEDS_BIN}"
cp bin/icuin44.dll "${SGNEEDS_BIN}"
cp lib/icudt.lib "${SGNEEDS_LIB}"
cp lib/icuuc.lib "${SGNEEDS_LIB}"
cp lib/icuin.lib "${SGNEEDS_LIB}"
cp include/unicode/* "${SGNEEDS_INCLUDE}"/unicode



################################################################
cd "${ROOT}"
echo
echo Building sgneeds package
MY_ZIP=sgneeds_`date "+%Y_%m_%d_%H%M"`.zip
zip -r ${MY_ZIP} sgneeds
echo Built libraries are in ${MY_ZIP}

echo '(1) cd to c:\ and unzip this file there'
echo '(2) put c:\sgneeds\bin in your path'


