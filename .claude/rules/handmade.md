# Handmade style (overrides generic C++ rules)
This rule gives the code style of hexdom. It overrides the generic C++ rules.
- Unity build: build.bat compiles two translation units (win32_hexdom.cpp,
  hexdom.cpp) and one test exe. No CMake, no project files.
- C-style C++: structs + free functions. No classes with virtuals, no
  exceptions (/EHsc-), no RTTI (/GR-), no STL containers in code/sim and
  code/ecs.
- Memory: the platform layer gives one block, and the game carves it into
  arenas (permanent, transient, frame). No malloc/new in the game DLL after
  startup. Dear ImGui allocates through the allocators of the platform
  layer; that is the one exception.
- Fixed-size, cache-friendly arrays; struct-of-arrays for hot component data.
- internal / global_variable / local_persist macros as in Handmade Hero.
  u8..u64, s8..s64, f32, f64, b32 typedefs in code/base.h.
- /W4 /WX. Warnings are errors.
- Debug UI and tools are ImGui, in code/ui only.
- Use templates only to generate typed arrays (component_table<T>).
