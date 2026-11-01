@echo off
REM Runs the test pyramid bottom to top and stops at the first failing layer.
REM   run_tests.bat          unit -> feature -> ui (headless)
REM   run_tests.bat --e2e    also runs the window for 120 frames and checks its log
REM   run_tests.bat --demo   only the demo walkthroughs: records build\demos\<test>.mp4 (or .gif)
REM                          and a PNG of each panel; builds nothing
REM   run_tests.bat --demo-fast  the same walkthroughs and checks in the fast mode of demo_player.exe:
REM                          no video, no PNG, no vsync; builds nothing
REM   run_tests.bat --web    only the unit and feature tests of build_web.bat, under node
REM   run_tests.bat --server only the tests of the room server (tests\server), with Python
setlocal
set ROOT=%~dp0
if /i "%~1"=="--server" goto :server
if not exist "%ROOT%build\unit_tests.exe" (
  echo run_tests.bat: build first ^(build.bat^)
  exit /b 1
)
pushd "%ROOT%build"
if /i "%~1"=="--demo" goto :demo
if /i "%~1"=="--demo-fast" goto :demo_fast
if /i "%~1"=="--web" goto :web

echo === unit ===
.\unit_tests.exe
if errorlevel 1 goto :fail

echo === feature ===
.\feature_tests.exe
if errorlevel 1 goto :fail

echo === ui ===
.\ui_tests.exe
if errorlevel 1 goto :fail

if /i "%~1"=="--e2e" (
  echo === e2e: window smoke run ===
  .\win32_hexdom.exe --frames 120
  if errorlevel 1 goto :fail
  findstr /c:"clean exit" hexdom.log >nul
  if errorlevel 1 goto :fail
  echo PASS window smoke run
)

popd
echo ALL PASS
exit /b 0

REM The tests of the Python server of the site (tests\server). The tests of the
REM rooms need no build. The tests of the referee (MP6) need build\referee.js
REM (build_web.bat referee) and the fixture that feature_tests.exe writes.
:server
if "%HEXDOM_PYTHON%"=="" set HEXDOM_PYTHON=C:\tools\hexdom-venv\Scripts\python.exe
echo === server ===
"%HEXDOM_PYTHON%" -I "%ROOT%tests\server\rooms_tests.py"
if errorlevel 1 (
  echo TESTS FAILED
  exit /b 1
)
echo === discord ===
"%HEXDOM_PYTHON%" -I "%ROOT%tests\server\discord_tests.py"
if errorlevel 1 (
  echo TESTS FAILED
  exit /b 1
)
echo === email link ===
"%HEXDOM_PYTHON%" -I "%ROOT%tests\server\email_link_tests.py"
if errorlevel 1 (
  echo TESTS FAILED
  exit /b 1
)
echo === referee ===
"%HEXDOM_PYTHON%" -I "%ROOT%tests\server\referee_tests.py"
if errorlevel 1 (
  echo TESTS FAILED
  exit /b 1
)
echo ALL PASS
exit /b 0

REM The unit tests and the feature tests of build_web.bat, as WebAssembly under
REM node (docs/WEB.md). They run in build\, so they find the files as on Windows.
:web
if not exist unit_tests.js (
  echo run_tests.bat: build first ^(build_web.bat^)
  goto :fail
)
set NODE=node
where node >nul 2>&1
if errorlevel 1 for /d %%n in ("%EMSDK%\node\*" "C:\tools\emsdk\node\*") do if exist "%%n\bin\node.exe" set NODE="%%n\bin\node.exe"
echo === unit (web) ===
%NODE% unit_tests.js
if errorlevel 1 goto :fail
echo === feature (web) ===
%NODE% feature_tests.js
if errorlevel 1 goto :fail
popd
echo ALL PASS
exit /b 0

REM One demo_player.exe run for each demo test. A test fails if it fails in the
REM player, or if its video or its PNG files are not in build\demos after the run.
:demo
if not exist demo_player.exe (
  echo run_tests.bat: build first ^(build.bat^)
  goto :fail
)
set DEMO_COUNT=0
for /f "delims=" %%t in ('.\demo_player.exe --list') do (
  set /a DEMO_COUNT+=1
  echo === demo: %%t ===
  del /q "demos\%%t.mp4" "demos\%%t.gif" "demos\%%t_*.png" >nul 2>&1
  .\demo_player.exe %%t
  if errorlevel 1 goto :fail
  if not exist "demos\%%t.mp4" if not exist "demos\%%t.gif" (
    echo FAIL %%t: no video in build\demos
    goto :fail
  )
  if not exist "demos\%%t_*.png" (
    echo FAIL %%t: no PNG in build\demos
    goto :fail
  )
  echo PASS %%t
)
if %DEMO_COUNT%==0 (
  echo FAIL no demo tests
  goto :fail
)
popd
echo ALL PASS
exit /b 0

REM The fast mode: one demo_player.exe run for all demo tests, with the same checks.
REM It records nothing, so it does not look in build\demos.
:demo_fast
if not exist demo_player.exe (
  echo run_tests.bat: build first ^(build.bat^)
  goto :fail
)
echo === demo (fast) ===
.\demo_player.exe --fast
if errorlevel 1 goto :fail
popd
echo ALL PASS
exit /b 0

:fail
popd
echo TESTS FAILED
exit /b 1
