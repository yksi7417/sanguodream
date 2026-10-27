#ifndef WASM_IMCONFIG_H
#define WASM_IMCONFIG_H
/* The ImGui settings of the web build. build_web.bat includes this file
   first in each unit, the ImGui lib and the game, so all of them agree.

   The draw list of the main map has more than 65536 vertices. The D3D11
   backend draws such a list with a vertex offset. WebGL2 has no draw with
   a base vertex, so the OpenGL3 backend cannot. 32-bit indices fix this,
   and WebGL2 draws them. */
#define ImDrawIdx unsigned int

#endif /* WASM_IMCONFIG_H */
