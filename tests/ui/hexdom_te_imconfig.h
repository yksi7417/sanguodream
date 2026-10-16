/* ImGui configuration for the UI test build only (imgui_te.lib and
   ui_tests.exe). The game's own ImGui (imgui.lib) uses the stock imconfig.h,
   so the test engine hooks never ship in hexdom.dll or win32_hexdom.exe. */
#pragma once

#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1
#define IMGUI_TEST_ENGINE_ENABLE_CAPTURE 0
#define IMGUI_TEST_ENGINE_ENABLE_IMPLOT 0
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

/* A failed IM_ASSERT logs the running test's name, then breaks. */
#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) IM_TEST_ENGINE_ASSERT(_EXPR)
#endif

#include "imgui_test_engine/imgui_te_imconfig.h"
