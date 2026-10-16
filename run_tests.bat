@echo off
REM Runs the test pyramid bottom to top and stops at the first failing layer.
REM   run_tests.bat          unit -> feature -> ui (headless)
REM   run_tests.bat --e2e    also runs the window for 120 frames and checks its log
REM   run_tests.bat --demo   only the demo walkthroughs: records build\demos\<test>.mp4 (or .gif)
REM                          and a PNG of each panel; builds nothing
setlocal
set ROOT=%~dp0
if not exist "%ROOT%build\unit_tests.exe" (
  echo run_tests.bat: build first ^(build.bat^)
  exit /b 1
)
pushd "%ROOT%build"
if /i "%~1"=="--demo" goto :demo

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

:fail
popd
echo TESTS FAILED
exit /b 1
