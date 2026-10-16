/* ImGui configuration for the test engine build only (imgui_te.lib,
   ui_tests.exe and demo_player.exe). The game's own ImGui (imgui.lib) uses
   the stock imconfig.h, so the test engine hooks never ship in hexdom.dll or
   win32_hexdom.exe. The capture tool is on for demo_player.exe: it writes
   PNG files through stb_image_write and videos through an encoder exe. */
#pragma once

#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1
#define IMGUI_TEST_ENGINE_ENABLE_CAPTURE 1
#define IMGUI_TEST_ENGINE_ENABLE_IMPLOT 0
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

/* A failed IM_ASSERT logs the running test's name, then breaks. */
#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) IM_TEST_ENGINE_ASSERT(_EXPR)
#endif

#include "imgui_test_engine/imgui_te_imconfig.h"
