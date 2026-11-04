/* The effects on the map (the player, 2026-10-28): flames on a hex that
   burns, and a spiral that turns over the head of a unit that is dizzy.
   - A fire stays with its hex. A land unit that leaves the hex leaves the
     flames there.
   - A ship carries its fire: water does not burn without a unit
     (terrain.burnable, F-149), so the flames go with the ship.
   - Only a confused unit (混亂) is dizzy. The spiral goes with the unit.
     Fire and confusion are independent (the player, 2026-11-01).
   - A unit with the trait fire_immune shows no flames on its hex.
   The effects ask the engine for the fire, the terrain and the units, and
   command nothing. Included by ui.cpp after ui_locate.cpp. */

#define UI_EFFECT_CAPACITY      512
#define UI_EFFECT_SHIP_CAPACITY 64
#define UI_EFFECT_DIZZY_LIFT    1.1f    /* world units: the spiral turns under the plate of the unit (UI_UNIT_PLATE_LIFT) */
#define UI_EFFECT_FLAME_HEIGHT  0.95f   /* world units: the height of a flame (a hex is 2 wide) */

enum ui_effect_kind
{
    UI_EFFECT_FIRE_HEX,    /* flames on a hex */
    UI_EFFECT_FIRE_UNIT,   /* flames on a ship: they go with the unit */
    UI_EFFECT_DIZZY,       /* a spiral over a unit */
};

struct ui_effect
{
    u8        kind;
    hex_coord hex;    /* UI_EFFECT_FIRE_HEX: the hex */
    u32       unit;   /* the other kinds: the unit */
};

/* True if the cell of a hex burns now. */
internal b32
ui_effect_burns(engine_fire_view fire, hex_coord hex)
{
    hex_offset o = hex_to_offset(hex);
    if (!fire.turns || o.col < 0 || o.row < 0 || (u32)o.col >= fire.width || (u32)o.row >= fire.height) return false;
    return fire.turns[(u32)o.row * fire.width + (u32)o.col] != 0;
}

/* True if a fire stays on the terrain of a hex without a unit. */
internal b32
ui_effect_burnable(engine_api *api, engine_terrain_view view, hex_coord hex)
{
    hex_offset o = hex_to_offset(hex);
    if (!view.kind || o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) return true;
    return api->terrain_kind_at(api->engine, view.kind[(u32)o.row * view.width + (u32)o.col]).burnable;
}

/* The effects of this frame, in out. Returns the count. */
internal u32
ui_scene_effects(engine_api *api, ui_effect *out, u32 capacity)
{
    engine_fire_view fire = api->fire_view(api->engine);
    engine_terrain_view view = api->terrain_view(api->engine);
    hex_coord ships[UI_EFFECT_SHIP_CAPACITY];
    u32 ship_count = 0, count = 0;
    for (u32 id = 0; id < api->unit_count(api->engine) && count < capacity; ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        if (unit.id == ENGINE_NO_UNIT) continue;
        hex_coord hex = hex_make(unit.q, unit.r);
        b32 on_fire = ui_effect_burns(fire, hex);
        if (on_fire && !unit.fire_immune && !ui_effect_burnable(api, view, hex) && count < capacity) out[count++] = { (u8)UI_EFFECT_FIRE_UNIT, hex, unit.id };
        if (on_fire && (unit.fire_immune || !ui_effect_burnable(api, view, hex)) && ship_count < UI_EFFECT_SHIP_CAPACITY) ships[ship_count++] = hex;
        if (unit.state == UNIT_STATE_CONFUSED && count < capacity) out[count++] = { (u8)UI_EFFECT_DIZZY, hex, unit.id };
    }
    for (u32 row = 0; fire.turns && row < fire.height; ++row)
    {
        for (u32 col = 0; col < fire.width && count < capacity; ++col)
        {
            if (!fire.turns[row * fire.width + col]) continue;
            hex_offset o = { (s32)col, (s32)row };
            hex_coord hex = hex_from_offset(o);
            b32 carried = false;
            for (u32 index = 0; index < ship_count; ++index) carried |= ships[index].q == hex.q && ships[index].r == hex.r;
            if (!carried) out[count++] = { (u8)UI_EFFECT_FIRE_HEX, hex, ENGINE_NO_UNIT };
        }
    }
    return count;
}

/* The screen length of one world unit up from a point, for the size of an effect. */
internal f32
ui_effect_pixels(m4 view_projection, v3 at)
{
    ImGuiIO &io = ImGui::GetIO();
    f32 x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    v3 up = at;
    up.y += 1.0f;
    if (!camera_project(view_projection, at, io.DisplaySize.x, io.DisplaySize.y, &x0, &y0) ||
        !camera_project(view_projection, up, io.DisplaySize.x, io.DisplaySize.y, &x1, &y1)) return 0.0f;
    return sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
}

/* The rows of the effect atlas of a pack: data/<pack>/fx.png, from
   misc/fx_import.py (docs/ASSETS.md, "Effects and icons"). Today the map
   draws the fires and the sign of confusion. The other rows wait for their
   choreography (docs/GRAPHICS.md, section 8). */
enum ui_fx_row
{
    UI_FX_BIG_FIRE, UI_FX_FLAME_LOOP, UI_FX_SMOKE_PUFF, UI_FX_DUST_CLOUD, UI_FX_IMPACT_DEBRIS, UI_FX_SPARKS, UI_FX_WATER_SPLASH,
    UI_FX_THUNDERBOLT, UI_FX_DARK_VORTEX, UI_FX_CALM_RIPPLE, UI_FX_DRUM_RINGS, UI_FX_PROJECTILES, UI_FX_ICONS,
    UI_FX_ROW_COUNT
};
#define UI_FX_COLUMNS       8
#define UI_FX_FLAME_FRAMES  8
#define UI_FX_FLAME_RATE    10.0f   /* frames each second */
#define UI_FX_ICON_CONFUSION 0      /* the column of the spiral in the row of the icons */
#define UI_FX_FIRE_SCALE    1.7f    /* the cell over the height of a flame: a figure does not fill its cell */

/* The atlas of the game pack, or no texture. */
internal platform_picture
ui_fx_atlas(ui_state *ui)
{
    platform_picture none = {};
    char file[TEXT_PACK_SIZE + 16];
    if (!ui->data_picture || !text_game_pack_picture(ui->text, "fx.png", file, sizeof(file))) return none;
    return ui->data_picture(file);
}

/* One looping fire of the atlas. (x, y) is its base on the screen. */
internal void
ui_draw_fx_fire(ImDrawList *draw, platform_picture atlas, u32 row, f32 x, f32 y, f32 pixels, f32 seed, f32 time)
{
    f32 size = pixels * UI_EFFECT_FLAME_HEIGHT * UI_FX_FIRE_SCALE;
    if (size < 6.0f) return;   /* too far: a flame of a few pixels only adds noise */
    u32 frame = (u32)(time * UI_FX_FLAME_RATE + seed) % UI_FX_FLAME_FRAMES;
    ImVec2 uv_min((f32)frame / UI_FX_COLUMNS, (f32)row / UI_FX_ROW_COUNT);
    ImVec2 uv_max((f32)(frame + 1) / UI_FX_COLUMNS, (f32)(row + 1) / UI_FX_ROW_COUNT);
    draw->AddImage((ImTextureID)atlas.texture, ImVec2(x - size * 0.5f, y - size), ImVec2(x + size * 0.5f, y), uv_min, uv_max);
}

/* The badge of confusion of the atlas: it turns over the head of a unit. */
internal void
ui_draw_fx_dizzy(ImDrawList *draw, platform_picture atlas, f32 x, f32 y, f32 pixels, f32 time)
{
    f32 radius = pixels * 0.38f;
    if (radius < 9.0f) radius = 9.0f;
    f32 spin = -time * 3.0f;
    f32 across = cosf(spin) * radius, down = sinf(spin) * radius;
    f32 u0 = (f32)UI_FX_ICON_CONFUSION / UI_FX_COLUMNS, u1 = (f32)(UI_FX_ICON_CONFUSION + 1) / UI_FX_COLUMNS;
    f32 v0 = (f32)UI_FX_ICONS / UI_FX_ROW_COUNT, v1 = (f32)(UI_FX_ICONS + 1) / UI_FX_ROW_COUNT;
    draw->AddImageQuad((ImTextureID)atlas.texture,
                       ImVec2(x - across + down, y - down - across), ImVec2(x + across + down, y + down - across),
                       ImVec2(x + across - down, y + down + across), ImVec2(x - across - down, y - down + across),
                       ImVec2(u0, v0), ImVec2(u1, v0), ImVec2(u1, v1), ImVec2(u0, v1));
}

/* One flame: a tongue that sways and flickers, orange with a yellow core.
   (x, y) is its base on the screen, height its full height in pixels. */
internal void
ui_draw_flame(ImDrawList *draw, f32 x, f32 y, f32 height, f32 phase, f32 time)
{
    f32 flicker = 0.8f + 0.2f * sinf(time * 11.0f + phase * 3.1f) + 0.08f * sinf(time * 23.0f + phase);
    f32 tall = height * flicker, wide = height * 0.42f;
    f32 sway = height * 0.12f * sinf(time * 5.0f + phase * 1.7f);
    ImVec2 outer[5] = { ImVec2(x - wide * 0.5f, y), ImVec2(x - wide * 0.38f, y - tall * 0.45f), ImVec2(x + sway, y - tall),
                        ImVec2(x + wide * 0.38f, y - tall * 0.45f), ImVec2(x + wide * 0.5f, y) };
    draw->AddConvexPolyFilled(outer, 5, IM_COL32(240, 90, 20, 210));
    f32 core = 0.55f;
    ImVec2 inner[5] = { ImVec2(x - wide * 0.5f * core, y), ImVec2(x - wide * 0.38f * core, y - tall * 0.45f * core), ImVec2(x + sway * core, y - tall * core),
                        ImVec2(x + wide * 0.38f * core, y - tall * 0.45f * core), ImVec2(x + wide * 0.5f * core, y) };
    draw->AddConvexPolyFilled(inner, 5, IM_COL32(255, 220, 90, 235));
}

/* A group of flames round a point: a glow on the ground and some tongues. */
internal void
ui_draw_flames(ImDrawList *draw, f32 x, f32 y, f32 pixels, u32 tongues, f32 seed, f32 time)
{
    f32 height = pixels * UI_EFFECT_FLAME_HEIGHT;
    if (height < 6.0f) return;   /* too far: a flame of a few pixels only adds noise */
    draw->AddCircleFilled(ImVec2(x, y - height * 0.15f), height * 0.6f, IM_COL32(255, 80, 0, 60));
    for (u32 tongue = 0; tongue < tongues; ++tongue)
    {
        f32 phase = seed + (f32)tongue * 1.37f;
        f32 spread = ((f32)tongue - (f32)(tongues - 1) * 0.5f) * height * 0.38f;
        f32 back = (tongue % 2) ? -height * 0.08f : height * 0.05f;
        ui_draw_flame(draw, x + spread, y + back, height * (tongue == tongues / 2 ? 1.0f : 0.75f), phase, time);
    }
}

#define UI_DIZZY_SPIRAL_POINTS 40
#define UI_DIZZY_SPIRAL_TURNS  2.5f

/* A spiral that turns over the head of a unit: the sign of confusion. It
   faces the camera, with a dark outline under a light line, so that it
   shows on each terrain. */
internal void
ui_draw_dizzy(ImDrawList *draw, f32 x, f32 y, f32 pixels, f32 time)
{
    f32 radius = pixels * 0.38f;
    if (radius < 7.0f) radius = 7.0f;
    f32 thickness = radius * 0.16f;
    if (thickness < 1.5f) thickness = 1.5f;
    ImVec2 points[UI_DIZZY_SPIRAL_POINTS];
    f32 spin = -time * 5.0f;
    for (u32 index = 0; index < UI_DIZZY_SPIRAL_POINTS; ++index)
    {
        f32 along = (f32)index / (f32)(UI_DIZZY_SPIRAL_POINTS - 1);   /* 0 at the centre, 1 at the outer end */
        f32 angle = spin + along * UI_DIZZY_SPIRAL_TURNS * 2.0f * RENDER_PI;
        points[index] = ImVec2(x + cosf(angle) * radius * along, y + sinf(angle) * radius * along);
    }
    draw->AddPolyline(points, UI_DIZZY_SPIRAL_POINTS, IM_COL32(30, 20, 10, 200), ImDrawFlags_None, thickness + 2.0f);
    draw->AddPolyline(points, UI_DIZZY_SPIRAL_POINTS, IM_COL32(255, 235, 120, 255), ImDrawFlags_None, thickness);
}

/* Draws the effects of the map on the background, under the plates. */
internal void
ui_draw_scene_effects(ui_state *ui, engine_api *api, m4 view_projection, ImDrawList *draw)
{
    ImGuiIO &io = ImGui::GetIO();
    local_persist ui_effect effects[UI_EFFECT_CAPACITY];
    u32 count = ui_scene_effects(api, effects, UI_EFFECT_CAPACITY);
    f32 time = (f32)ImGui::GetTime();
    platform_picture atlas = count ? ui_fx_atlas(ui) : platform_picture{};
    for (u32 index = 0; index < count; ++index)
    {
        ui_effect *effect = effects + index;
        hex_coord hex = effect->hex;
        if (effect->kind != UI_EFFECT_FIRE_HEX)
        {
            engine_unit unit = api->unit_at(api->engine, effect->unit);
            if (unit.id == ENGINE_NO_UNIT) continue;
            hex = ui_round_unit_hex(ui, &unit);   /* M20: the effect walks with the unit */
        }
        v3 at = ui_hex_world(api, hex);
        if (effect->kind == UI_EFFECT_DIZZY) at.y += UI_EFFECT_DIZZY_LIFT;
        f32 x = 0.0f, y = 0.0f;
        if (!camera_project(view_projection, at, io.DisplaySize.x, io.DisplaySize.y, &x, &y)) continue;
        if (x < -64.0f || y < -64.0f || x > io.DisplaySize.x + 64.0f || y > io.DisplaySize.y + 64.0f) continue;
        f32 pixels = ui_effect_pixels(view_projection, at);
        f32 seed = (f32)((hex.q * 7 + hex.r * 13) & 31);
        if (atlas.texture)
        {
            if (effect->kind == UI_EFFECT_FIRE_HEX)       ui_draw_fx_fire(draw, atlas, UI_FX_BIG_FIRE, x, y, pixels, seed, time);
            else if (effect->kind == UI_EFFECT_FIRE_UNIT) ui_draw_fx_fire(draw, atlas, UI_FX_FLAME_LOOP, x, y, pixels, seed, time);
            else                                          ui_draw_fx_dizzy(draw, atlas, x, y, pixels, time);
            continue;
        }
        switch (effect->kind)
        {
            case UI_EFFECT_FIRE_HEX:  ui_draw_flames(draw, x, y, pixels, 5, seed, time); break;
            case UI_EFFECT_FIRE_UNIT: ui_draw_flames(draw, x, y, pixels, 3, seed, time); break;
            default:                  ui_draw_dizzy(draw, x, y, pixels, time); break;
        }
    }
}
