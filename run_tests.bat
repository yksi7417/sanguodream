@echo off
REM Runs the test pyramid bottom to top and stops at the first failing layer.
REM   run_tests.bat          unit -> feature -> ui (headless)
REM   run_tests.bat --e2e    also runs the window for 120 frames and checks its log
setlocal
set ROOT=%~dp0
if not exist "%ROOT%build\unit_tests.exe" (
  echo run_tests.bat: build first ^(build.bat^)
  exit /b 1
)
pushd "%ROOT%build"

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

:fail
popd
echo TESTS FAILED
exit /b 1
