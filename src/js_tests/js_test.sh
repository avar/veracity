#!/bin/sh
## usage: js_test.sh ${CMAKE_CURRENT_SOURCE_DIR} stTestName.js
set -o xtrace

export SRCDIR=$1
##echo SRCDIR is $SRCDIR

export TEST=$2
##echo TEST is $TEST

export BINDIR=`pwd`
##echo BINDIR is $BINDIR

## We need vv and vscript to be in our path.
export PATH=$BINDIR/../cmd:$BINDIR/../script:$PATH
##echo PATH is $PATH

## The "output" console log is always appended rather than starting
## with a newly created/truncated file.  Get rid of the noise from
## the previous run.

if [ -e $BINDIR/output.$TEST.log ]; then
    rm $BINDIR/output.$TEST.log
fi

## Since vscript does not take pathnames, only a list of .js files in the current directory,
## we have to do a little gymnastics to get the scripts to run using the source directory
## as input and the build directory for all of the output and trash we generate.

cd $SRCDIR
vscript vscript_tests.js --load $TEST -v --temp $BINDIR/temp -o $BINDIR/output.$TEST.log >$BINDIR/stdout.$TEST.log 2>&1
RESULT_TEST=$?

cd $BINDIR
#rm -rf $BINDIR/temp

## If the test failed (exit status > 0), just exit now.
if [ $RESULT_TEST -ne 0 ]; then
    exit $RESULT_TEST;
fi

## Check for memory leaks in the log file.  egrep reports an error (1) if our pattern WAS NOT found.
## So we get a success (0) status if there were leaks.
##
## if this is stWeb.js, we have background server processes and sometimes they take a little
## time to exit and flush their output to stdout/stderr.  if we do the grep too quickly (at least
## on the Mac), we blow past it before the leaks get written to the file (or something like that).
if [ $TEST == stWeb.js ]; then
    sleep 60;
fi
grep "LEAK:" $BINDIR/stdout.$TEST.log
RESULT_LEAK=$?
if [ $RESULT_LEAK -eq 0 ]; then
    exit 1;
fi

exit 0
