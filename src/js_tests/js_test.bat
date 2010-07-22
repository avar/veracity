@ECHO OFF
REM js_test.bat
REM usage: ${CMAKE_CURRENT_SOURCE_DIR} stTestName.js ${MY_RELEXE_DIR}
REM

SET SRCDIR=%1%
ECHO SRCDIR is %SRCDIR%

SET TEST=%2%
ECHO TEST is %TEST%

SET RELEXEDIR=%3%
ECHO RELEXEDIR is %RELEXEDIR%

REM %cd% is like `pwd`
SET BINDIR=%cd%
ECHO BINDIR is %BINDIR%

REM We need vv and vscript to be in our path.
SET PATH=%BINDIR%\..\cmd\%RELEXEDIR%;%BINDIR%\..\script\%RELEXEDIR%;%PATH%
ECHO Path is %PATH%

REM The "output" console log is always appended rather than starting
REM with a newly created/truncated file.  Get rid of the noise from
REM the previous run.

if exist "%BINDIR%\output.%TEST%.log" del "%BINDIR%\output.%TEST%.log"

REM Since vscript does not take parameters, only a list of .js files in the current directory,
REM we have to do a little gymnastics to get the scripts to run using the source directory as
REM input and the build directory for all of the output and trash we generate.

REM cd /D changes both pathname and the disk in one step.
cd /D %SRCDIR%
ECHO Now at %cd%

ECHO Running test %TEST%...
vscript.exe vscript_tests.js --load %TEST% -v --temp %BINDIR%\temp -o %BINDIR%\output.%TEST%.log > %BINDIR%\stdout.%TEST%.log 2>&1
SET RESULT_TEST=%ERRORLEVEL%

REM cd back to bin directory and cleanup the trash
cd /D %BINDIR%
ECHO Now at %cd%

DEL /S /Q %BINDIR%\temp

REM If the test failed (exit status > 0), just exit now (without looking for leaks).
IF NOT "%RESULT_TEST%"=="0" EXIT %RESULT_TEST%

REM Check for memory leaks in the log file.  FIND reports an error (1) if our pattern WAS NOT found.
REM So we get a success (0) status if there were leaks.
REM 
REM if this is stWeb.js, we have background server processes and sometimes they take a little
REM time to exit and flush their output to stdout/stderr.  if we do the grep too quickly (at least
REM on the Mac), we blow past it before the leaks get written to the file (or something like that).
if /i "%TEST%"=="stWeb.js" ping -n 1 -w 60000 224.0.0.0

ECHO Looking for leaks...
FIND "LEAK:" %BINDIR%\stdout.%TEST%.log
SET RESULT_LEAK=%ERRORLEVEL%
IF "%RESULT_LEAK%"=="0" EXIT 1

ECHO Everything passed.
EXIT 0



