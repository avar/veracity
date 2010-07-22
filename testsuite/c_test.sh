#!/bin/sh
## usage: js_test.sh c_test_name ${CMAKE_CURRENT_SOURCE_DIR}
set -o xtrace

export TEST=$1
##echo TEST is $TEST

export SRCDIR=$2
##echo SRCDIR is $SRCDIR

export BINDIR=`pwd`
##echo BINDIR is $BINDIR

## We need vv to be in our path.
export PATH=$BINDIR/../src/cmd:$PATH
##echo PATH is $PATH

./$TEST "${SRCDIR}"
RESULT_TEST=$?

exit $RESULT_TEST;
