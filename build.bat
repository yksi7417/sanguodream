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
set INCLUDES=/I%ROOT%code /I%ROOT%tests /I%ROOT%third_party\imgui /I%ROOT%third_party\imgui\backends
set COMMON=/nologo /Zi /Od %WARN% /EHsc- /GR- /MT /FC /DHEXDOM_INTERNAL=1 /DHEXDOM_SLOW=1 %INCLUDES%
REM UI tests: ImGui built with test-engine hooks, exceptions on (the test engine's coroutine uses std::thread).
set TE_INCLUDES=/I%ROOT%tests\ui /I%ROOT%third_party\imgui_test_engine
set TE_DEFINES=/DIMGUI_USER_CONFIG=\"hexdom_te_imconfig.h\"
set COMMON_TE=/nologo /Zi /Od %WARN% /EHsc /GR- /MT /FC /DHEXDOM_INTERNAL=1 /DHEXDOM_SLOW=1 %INCLUDES% %TE_INCLUDES% %TE_DEFINES%
REM Not named LINK: the linker reads an environment variable of that name.
set LINKFLAGS=/link /INCREMENTAL:NO /opt:ref
if not exist "%ROOT%build" mkdir "%ROOT%build"
pushd "%ROOT%build"
set ERR=0
echo building > lock.tmp

REM The static libs carry their debug information in the objects (/Z7). With /Zi
REM they point at build\vc140.pdb, and each later compile replaces that file, so
REM the linker warned LNK4204 for each object of the libs. A change of the lib
REM flags below rebuilds both libs through the stamp file lib_flags.txt.
set LIBFLAGS=/nologo /Z7 /O2 /W3 /GR- /MT /c
echo %LIBFLAGS%> lib_flags.new
fc lib_flags.new lib_flags.txt >nul 2>&1
if errorlevel 1 (
  del imgui.lib >nul 2>&1
  del imgui_te.lib >nul 2>&1
  del te\*.obj >nul 2>&1
  copy /y lib_flags.new lib_flags.txt >nul
)
del lib_flags.new >nul 2>&1

REM Dear ImGui static lib for the game, built once. Delete build\imgui.lib to rebuild it.
if not exist imgui.lib (
  cl %LIBFLAGS% /EHsc- /I..\third_party\imgui /I..\third_party\imgui\backends ^
    ..\third_party\imgui\imgui.cpp ..\third_party\imgui\imgui_draw.cpp ..\third_party\imgui\imgui_tables.cpp ^
    ..\third_party\imgui\imgui_widgets.cpp ..\third_party\imgui\imgui_demo.cpp ^
    ..\third_party\imgui\backends\imgui_impl_win32.cpp ..\third_party\imgui\backends\imgui_impl_dx11.cpp
  if errorlevel 1 set ERR=1
  lib /nologo /out:imgui.lib imgui.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj imgui_demo.obj imgui_impl_win32.obj imgui_impl_dx11.obj
  if errorlevel 1 set ERR=1
)

REM Dear ImGui + Test Engine + the Win32 and DX11 backends, for ui_tests.exe and demo_player.exe.
REM Built once. "build.bat te" rebuilds it, and so does a change to tests\ui\hexdom_te_imconfig.h.
if /i "%~1"=="te" (
  del imgui_te.lib >nul 2>&1
  del te\*.obj >nul 2>&1
)
if exist imgui_te.lib (
  fc /b ..\tests\ui\hexdom_te_imconfig.h te\hexdom_te_imconfig.h >nul 2>&1
  if errorlevel 1 (
    del imgui_te.lib >nul 2>&1
    del te\*.obj >nul 2>&1
  )
)
if not exist imgui_te.lib (
  if not exist te mkdir te
  cl %LIBFLAGS% /EHsc /Fote\ %TE_INCLUDES% %TE_DEFINES% /I..\third_party\imgui ^
    ..\third_party\imgui\imgui.cpp ..\third_party\imgui\imgui_draw.cpp ..\third_party\imgui\imgui_tables.cpp ^
    ..\third_party\imgui\imgui_widgets.cpp ..\third_party\imgui\imgui_demo.cpp ^
    ..\third_party\imgui_test_engine\imgui_test_engine\imgui_te_context.cpp ^
    ..\third_party\imgui_test_engine\imgui_test_engine\imgui_te_coroutine.cpp ^
    ..\third_party\imgui_test_engine\imgui_test_engine\imgui_te_engine.cpp ^
    ..\third_party\imgui_test_engine\imgui_test_engine\imgui_te_exporters.cpp ^
    ..\third_party\imgui_test_engine\imgui_test_engine\imgui_te_perftool.cpp ^
    ..\third_party\imgui_test_engine\imgui_test_engine\imgui_te_ui.cpp ^
    ..\third_party\imgui_test_engine\imgui_test_engine\imgui_te_utils.cpp ^
    ..\third_party\imgui_test_engine\imgui_test_engine\imgui_capture_tool.cpp ^
    ..\third_party\imgui\backends\imgui_impl_win32.cpp ..\third_party\imgui\backends\imgui_impl_dx11.cpp
  if errorlevel 1 set ERR=1
  lib /nologo /out:imgui_te.lib te\*.obj
  if errorlevel 1 set ERR=1
  copy /y ..\tests\ui\hexdom_te_imconfig.h te\hexdom_te_imconfig.h >nul
)

REM Game DLL. Hot reloadable; unique PDB name so a debugger lets go of the old one.
del hexdom_*.pdb >nul 2>&1
cl %COMMON% /LD ..\code\hexdom.cpp /Fe:hexdom.dll %LINKFLAGS% /PDB:hexdom_%random%.pdb /EXPORT:GameUpdateAndRender imgui.lib
if errorlevel 1 set ERR=1

REM "build.bat dll" rebuilds only the DLL: the hot-reload loop while the window is open
REM (the exe cannot be relinked while it is running).
if /i "%~1"=="dll" goto :done

REM Platform layer.
cl %COMMON% ..\code\win32_hexdom.cpp /Fe:win32_hexdom.exe %LINKFLAGS% /SUBSYSTEM:WINDOWS imgui.lib user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 set ERR=1

REM The test pyramid, bottom to top.
cl %COMMON% ..\tests\unit\unit_main.cpp /Fe:unit_tests.exe %LINKFLAGS%
if errorlevel 1 set ERR=1
cl %COMMON% ..\tests\feature\feature_main.cpp /Fe:feature_tests.exe %LINKFLAGS%
if errorlevel 1 set ERR=1
cl %COMMON_TE% ..\tests\ui\ui_tests.cpp /Fe:ui_tests.exe %LINKFLAGS% imgui_te.lib
if errorlevel 1 set ERR=1

REM The demo recorder and end-to-end smoke test: a real window, the engine and the UI compiled in,
REM the "demo" tests at cinematic speed. gif_encode.exe is its video encoder if ffmpeg is missing.
cl %COMMON_TE% ..\tests\demo\demo_player.cpp /Fe:demo_player.exe %LINKFLAGS% imgui_te.lib user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 set ERR=1
cl %COMMON% ..\misc\gif_encode.cpp /Fe:gif_encode.exe %LINKFLAGS%
if errorlevel 1 set ERR=1

REM Tools: save_dump.exe prints a save as JSON with the clean-room loader.
cl %COMMON% ..\misc\save_dump.cpp /Fe:save_dump.exe %LINKFLAGS%
if errorlevel 1 set ERR=1

:done
del lock.tmp
popd
if %ERR% neq 0 echo build.bat: FAILED
exit /b %ERR%
