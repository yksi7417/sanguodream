@echo off
REM The web build (docs/WEB.md).
REM   build_web.bat         the game: build\web\index.html, .js, .wasm and .data
REM   build_web.bat tests   the unit tests and the feature tests as WebAssembly
REM                         for node: build\unit_tests.js and build\feature_tests.js.
REM                         "run_tests.bat --web" runs them.
REM It needs the Emscripten SDK in C:\tools\emsdk, or in EMSDK. The game also
REM needs build\web\fonts\NotoSansCJKtc-Regular.otf from misc\subset_font.py.
setlocal
if "%EMSDK%"=="" set EMSDK=C:\tools\emsdk
if not exist "%EMSDK%\emsdk_env.bat" (
  echo build_web.bat: no Emscripten SDK in %EMSDK%; see docs/WEB.md
  exit /b 1
)
call "%EMSDK%\emsdk_env.bat" >nul 2>&1
set ROOT=%~dp0
set INCLUDES=-I%ROOT%code -I%ROOT%tests -I%ROOT%third_party\imgui -I%ROOT%third_party\imgui\backends
REM The same warnings as build.bat /W4 /WX, less the ones that build.bat turns off,
REM and less one clang check that MSVC does not have: a u8 against a capacity of 256.
set WARN=-Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-missing-field-initializers -Wno-tautological-constant-out-of-range-compare -ferror-limit=0
set DEFINES=-DHEXDOM_INTERNAL=1 -DHEXDOM_SLOW=1
if not exist "%ROOT%build" mkdir "%ROOT%build"
pushd "%ROOT%build"
set ERR=0
if /i "%~1"=="tests" goto :tests

REM ---- the game ---------------------------------------------------------------
if not exist web\fonts\NotoSansCJKtc-Regular.otf (
  echo build_web.bat: no web font; run C:\tools\hexdom-venv\Scripts\python -I misc\subset_font.py
  goto :failed
)
REM Dear ImGui and its GLFW and OpenGL3 backends, built once into a static lib.
REM Delete build\web_obj to build it again, for example after a change to code\wasm_imconfig.h.
set IMGUI_FLAGS=-O2 -include ..\code\wasm_imconfig.h -I..\third_party\imgui -I..\third_party\imgui\backends
if not exist web_obj\imgui_web.a (
  if not exist web_obj mkdir web_obj
  for %%f in (imgui imgui_draw imgui_tables imgui_widgets imgui_demo) do (
    call em++ %IMGUI_FLAGS% -c ..\third_party\imgui\%%f.cpp -o web_obj\%%f.o
    if errorlevel 1 set ERR=1
  )
  for %%f in (imgui_impl_glfw imgui_impl_opengl3) do (
    call em++ %IMGUI_FLAGS% -c ..\third_party\imgui\backends\%%f.cpp -o web_obj\%%f.o
    if errorlevel 1 set ERR=1
  )
  call emar rcs web_obj\imgui_web.a web_obj\imgui.o web_obj\imgui_draw.o web_obj\imgui_tables.o web_obj\imgui_widgets.o web_obj\imgui_demo.o web_obj\imgui_impl_glfw.o web_obj\imgui_impl_opengl3.o
  if errorlevel 1 set ERR=1
)
if %ERR%==1 goto :failed

REM The files of the game, packed into index.data. The current folder of the game
REM is /build, so "../data/placeholder" is the same path as on Windows. The three
REM spec fixtures are the scenarios of the title menu (data/placeholder/scenarios.tsv).
set PACK=--preload-file ../data/placeholder@/data/placeholder ^
  --preload-file ../data/civil_war@/data/civil_war ^
  --preload-file web/fonts/NotoSansCJKtc-Regular.otf@/data/fonts/NotoSansCJKtc-Regular.otf ^
  --preload-file ../data/fonts/NotoSansThai-Regular.ttf@/data/fonts/NotoSansThai-Regular.ttf ^
  --preload-file ../spec-src/spec/fixtures/saves/sample.s11@/spec-src/spec/fixtures/saves/sample.s11 ^
  --preload-file ../spec-src/spec/fixtures/scenarios/SCEN002.s11@/spec-src/spec/fixtures/scenarios/SCEN002.s11 ^
  --preload-file ../spec-src/spec/fixtures/scenarios/Scenario.s11@/spec-src/spec/fixtures/scenarios/Scenario.s11
set GAME_LINK=-sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2GB ^
  -sINITIAL_MEMORY=64MB -sSTACK_SIZE=8MB -sFORCE_FILESYSTEM=1 -lidbfs.js --profiling-funcs ^
  --shell-file ..\web\shell.html
call em++ -std=c++17 -O2 -fno-exceptions -fno-rtti -include ..\code\wasm_imconfig.h %DEFINES% %WARN% %INCLUDES% ..\code\wasm_hexdom.cpp web_obj\imgui_web.a ^
  -o web\index.html %GAME_LINK% %PACK%
if errorlevel 1 set ERR=1
goto :done

REM ---- the tests --------------------------------------------------------------
:tests
REM NODERAWFS: the tests read the real files of the repository, as on Windows.
set FLAGS=-std=c++17 -O1 -g -fno-exceptions -fno-rtti %DEFINES% %WARN% %INCLUDES%
set LINKFLAGS=-sENVIRONMENT=node -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4GB -sSTACK_SIZE=8MB -sEXIT_RUNTIME=1
call em++ %FLAGS% ..\tests\unit\unit_main.cpp -o unit_tests.js %LINKFLAGS%
if errorlevel 1 set ERR=1
call em++ %FLAGS% ..\tests\feature\feature_main.cpp -o feature_tests.js %LINKFLAGS%
if errorlevel 1 set ERR=1

:done
popd
if %ERR%==1 (
  echo build_web.bat: FAILED
  exit /b 1
)
echo build_web.bat: done
exit /b 0

:failed
popd
echo build_web.bat: FAILED
exit /b 1
