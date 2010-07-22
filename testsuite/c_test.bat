@ECHO OFF
REM c_test.bat
REM usage: c_test_name ${CMAKE_CURRENT_SOURCE_DIR} ${MY_RELEXE_DIR}
REM

SET TEST=%1%
ECHO TEST is %TEST%

SET SRCDIR=%2%
ECHO SRCDIR is %SRCDIR%

SET RELEXEDIR=%3%
ECHO RELEXEDIR is %RELEXEDIR%

REM %cd% is like `pwd`
SET BINDIR=%cd%
ECHO BINDIR is %BINDIR%

REM We need vv to be in our path.
SET PATH=%BINDIR%\..\src\cmd\%RELEXEDIR%;%PATH%
ECHO Path is %PATH%

ECHO Running test %TEST%...
%RELEXEDIR%\%TEST% "%SRCDIR%"
SET RESULT_TEST=%ERRORLEVEL%

EXIT %RESULT_TEST%
