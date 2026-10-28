@echo off
REM Sets HEXDOM_BUILD_NUMBER (the count of the commits of git) and HEXDOM_BUILD_COMMIT
REM (the short hash of the commit) for build.bat and build_web.bat (code/version.h).
REM Without git, the values are 0 and dev.
set HEXDOM_BUILD_NUMBER=
set HEXDOM_BUILD_COMMIT=
for /f %%i in ('git -C "%~dp0.." rev-list --count HEAD 2^>nul') do set HEXDOM_BUILD_NUMBER=%%i
for /f %%i in ('git -C "%~dp0.." rev-parse --short^=7 HEAD 2^>nul') do set HEXDOM_BUILD_COMMIT=%%i
if not defined HEXDOM_BUILD_NUMBER set HEXDOM_BUILD_NUMBER=0
if not defined HEXDOM_BUILD_COMMIT set HEXDOM_BUILD_COMMIT=dev
exit /b 0
