@echo off
REM The web build (docs/WEB.md). For now it builds the unit tests and the
REM feature tests as WebAssembly for node: build\unit_tests.js and
REM build\feature_tests.js. "run_tests.bat --web" runs them.
REM It needs the Emscripten SDK in C:\tools\emsdk, or in EMSDK.
setlocal
if "%EMSDK%"=="" set EMSDK=C:\tools\emsdk
if not exist "%EMSDK%\emsdk_env.bat" (
  echo build_web.bat: no Emscripten SDK in %EMSDK%; see docs/WEB.md
  exit /b 1
)
call "%EMSDK%\emsdk_env.bat" >nul 2>&1
set ROOT=%~dp0
set INCLUDES=-I%ROOT%code -I%ROOT%tests -I%ROOT%third_party\imgui
REM The same warnings as build.bat /W4 /WX, less the ones that build.bat turns off.
set WARN=-Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-missing-field-initializers -Wno-tautological-constant-out-of-range-compare -ferror-limit=0
REM NODERAWFS: the tests read the real files of the repository, as on Windows.
set FLAGS=-std=c++17 -O1 -g -fno-exceptions -fno-rtti -DHEXDOM_INTERNAL=1 -DHEXDOM_SLOW=1 %WARN% %INCLUDES%
set LINKFLAGS=-sENVIRONMENT=node -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4GB -sSTACK_SIZE=8MB -sEXIT_RUNTIME=1
if not exist "%ROOT%build" mkdir "%ROOT%build"
pushd "%ROOT%build"
set ERR=0

call em++ %FLAGS% ..\tests\unit\unit_main.cpp -o unit_tests.js %LINKFLAGS%
if errorlevel 1 set ERR=1
call em++ %FLAGS% ..\tests\feature\feature_main.cpp -o feature_tests.js %LINKFLAGS%
if errorlevel 1 set ERR=1

popd
if %ERR%==1 (
  echo build_web.bat: FAILED
  exit /b 1
)
echo build_web.bat: done
exit /b 0
