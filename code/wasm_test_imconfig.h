#ifndef WASM_TEST_IMCONFIG_H
#define WASM_TEST_IMCONFIG_H
/* The ImGui settings of the test build of the web game ("build_web.bat
   test-game"). They are the settings of the web build, and ImGui calls
   the item hooks of code/wasm_test_hooks.h. */
#include "wasm_imconfig.h"
#define IMGUI_ENABLE_TEST_ENGINE

#endif /* WASM_TEST_IMCONFIG_H */
