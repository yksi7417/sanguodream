#ifndef UI_H
#define UI_H
/* Debug UI. It drives the backend only through engine_api
   (code/engine/engine.h); it never sees sim_state, the ECS or the ledger.
   ui_state lives in the permanent block so it survives DLL reloads. bool
   rather than b32 because ImGui writes these. */

#include "base.h"
#include "engine/engine.h"

struct ui_state
{
    bool show_demo;
    bool show_sim;
    bool show_ledger;
    bool show_memory;
    bool show_map;
    bool show_officers;
    bool show_cities;
    char load_message[96];   /* result of the last "Load sample save" click */
    char names_message[96];  /* result of the last "Load city names" click */
    struct
    {
        bool      has_from;
        bool      has_to;
        hex_coord from;
        hex_coord to;
        f32       zoom;
    } map;
};

/* Per-frame facts that belong to the front end, not the engine. */
struct ui_frame_info
{
    f32 dt_seconds;
    u32 dll_reload_count;
    u64 frame_arena_used;
    u64 frame_arena_size;
};

internal void ui_init(ui_state *ui);
internal void ui_draw(ui_state *ui, engine_api *api, ui_frame_info *frame);

#endif /* UI_H */
