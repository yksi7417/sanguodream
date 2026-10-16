@echo off
setlocal
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VCVARS% set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VCVARS% (
  echo build.bat: no vcvars64.bat found; install Visual Studio Build Tools with the C++ workload
  exit /b 1
)
call %VCVARS% >nul
if errorlevel 1 (
  echo build.bat: vcvars64.bat failed
  exit /b 1
)
set ROOT=%~dp0
set WARN=/W4 /WX /wd4100 /wd4127 /wd4189 /wd4201 /wd4505
set COMMON=/nologo /Zi /Od %WARN% /EHsc- /GR- /MT /FC /DHEXDOM_INTERNAL=1 /DHEXDOM_SLOW=1 /I%ROOT%code /I%ROOT%third_party\imgui /I%ROOT%third_party\imgui\backends
set LINK=/link /INCREMENTAL:NO /opt:ref
if not exist "%ROOT%build" mkdir "%ROOT%build"
pushd "%ROOT%build"
set ERR=0
echo building > lock.tmp

REM Dear ImGui static lib, built once. Delete build\imgui.lib to rebuild it.
if not exist imgui.lib (
  cl /nologo /Zi /O2 /W3 /EHsc- /GR- /MT /c /I..\third_party\imgui /I..\third_party\imgui\backends ^
    ..\third_party\imgui\imgui.cpp ..\third_party\imgui\imgui_draw.cpp ..\third_party\imgui\imgui_tables.cpp ^
    ..\third_party\imgui\imgui_widgets.cpp ..\third_party\imgui\imgui_demo.cpp ^
    ..\third_party\imgui\backends\imgui_impl_win32.cpp ..\third_party\imgui\backends\imgui_impl_dx11.cpp
  if errorlevel 1 set ERR=1
  lib /nologo /out:imgui.lib imgui.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj imgui_demo.obj imgui_impl_win32.obj imgui_impl_dx11.obj
  if errorlevel 1 set ERR=1
)

REM Game DLL. Hot reloadable; unique PDB name so a debugger lets go of the old one.
del hexdom_*.pdb >nul 2>&1
cl %COMMON% /LD ..\code\hexdom.cpp /Fe:hexdom.dll %LINK% /PDB:hexdom_%random%.pdb /EXPORT:GameUpdateAndRender imgui.lib
if errorlevel 1 set ERR=1

REM Platform layer.
cl %COMMON% ..\code\win32_hexdom.cpp /Fe:win32_hexdom.exe %LINK% /SUBSYSTEM:WINDOWS imgui.lib user32.lib gdi32.lib d3d11.lib dxgi.lib
if errorlevel 1 set ERR=1

REM Headless tests: sim and ecs only.
cl %COMMON% ..\tests\sim_tests.cpp /Fe:sim_tests.exe %LINK%
if errorlevel 1 set ERR=1

del lock.tmp
popd
if %ERR% neq 0 echo build.bat: FAILED
exit /b %ERR%
