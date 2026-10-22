/* Debug UI panels. The only place ImGui is called from inside the DLL, and
   the only thing it does to the game is call engine_api. */

#include <stdio.h>   /* snprintf for labels */
#include "imgui.h"
#include "ui/ui.h"

/* A label for a window, a menu item or a column: the text of the id in the
   active locale, then "###" and a stable id. ImGui and the tests find the
   item by the stable id, so the locale does not change its ImGui id. */
internal const char *
ui_label(char *out, u32 out_size, ui_state *ui, const char *text_id, const char *stable_id)
{
    snprintf(out, out_size, "%s###%s", text_get(ui->text, text_id), stable_id);
    return out;
}

#include "ui/ui_context.cpp"  /* the dialogs on demand of the game mode */
#include "ui/ui_facility_look.cpp"   /* the shape and the colour of each facility kind */
#include "ui/ui_map.cpp"
#include "ui/ui_world.cpp"   /* includes ui_table.cpp for its tables */
#include "ui/ui_settings.cpp"
#include "ui/ui_city.cpp"
#include "ui/ui_city_commands.cpp"
#include "ui/ui_officer_pick.cpp" /* M19: 選擇執行武將 */
#include "ui/ui_placement.cpp"    /* M19: the 開發 grid and the placement mode of the game mode */
#include "ui/ui_economy.cpp"
#include "ui/ui_growth.cpp"
#include "ui/ui_march.cpp"
#include "ui/ui_combat.cpp"
#include "ui/ui_content.cpp"  /* M16: before ui_court.cpp, for the portrait of the officer view */
#include "ui/ui_court.cpp"
#include "ui/ui_people.cpp"   /* clone gap report 2, group 3 */
#include "ui/ui_rivals.cpp"
#include "ui/ui_fidelity.cpp"  /* M13 */
#include "ui/ui_base_menu.cpp" /* M14: the base command menu of the game mode */
#include "ui/ui_feel.cpp"      /* M17: before ui_game.cpp, for the title and the load */
#include "ui/ui_game.cpp"

/* Writes an overlay colour for one cell of the terrain view, if it is on the map. */
internal void
ui_scene_mark(u32 *overlay, engine_terrain_view view, hex_coord h, u32 rgba)
{
    hex_offset o = hex_to_offset(h);
    if (o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) return;
    overlay[(u32)o.row * view.width + (u32)o.col] = rgba;
}

#include "ui/ui_scene.cpp"
#include "ui/ui_locate.cpp"   /* M11 */
#include "ui/ui_report.cpp"   /* M18: the log, the speech bubble and the values; needs ui_locate.cpp */
#include "ui/ui_round.cpp"    /* M20: the show of the AI round; needs ui_report.cpp */
#include "ui/ui_war.cpp"      /* the walk, the strikes and the hits; needs ui_round.cpp */
#include "ui/ui_minimap.cpp"  /* M12: the round mini map */
#include "ui/ui_hud.cpp"      /* the HUD and the list of the game mode */
#include "ui/ui_information.cpp" /* M21: the information menu, the tables and the detail panels */
#include "ui/ui_political.cpp"   /* M21: the political map 中地圖 and the hover panels */
#include "ui/ui_ai.cpp"       /* M15: needs ui_locate.cpp */

internal void
ui_init(ui_state *ui, text_catalog *text)
{
    ui->show_demo   = true;
    ui->show_sim    = true;
    ui->show_ledger = true;
    ui->show_memory = false;
    ui->show_map    = false;     /* the 2D panel stays available from the Windows menu */
    ui->show_officers = true;
    ui->show_cities   = true;
    ui->show_scene    = true;
    ui->show_settings = false;
    ui->show_city     = true;
    ui->show_develop  = false;
    ui->show_raw      = false;
    ui->show_all_cities = false;
    ui->font_size     = HEXDOM_FONT_DEFAULT_PIXELS;
    ui->officers_view = {};
    ui->cities_view   = {};
    ui->ledger_view   = {};
    ui->officers_view.sort_column = ui->cities_view.sort_column = ui->ledger_view.sort_column = UI_SORT_NO_COLUMN;
    ui->city.selected = 0;
    ui->develop = {};
    ui->develop.prepared_city = ENGINE_NO_CITY;
    ui->develop.slot          = ENGINE_ANY_SLOT;
    ui->show_picker           = false;   /* M19 */
    ui->picker                = {};
    ui->picker.view.sort_column = UI_SORT_NO_COLUMN;
    ui->placement             = {};
    ui->info                  = {};   /* M21 */
    ui->info.view.sort_column = UI_SORT_NO_COLUMN;
    ui->show_political        = false;
    ui->political             = {};
    ui->political.cities = ui->political.bases = ui->political.units = true;
    ui->round                 = {};   /* M20: the show is off by default */
    ui->round.force           = ENGINE_NO_FACTION;
    ui->round.move_unit       = ENGINE_NO_UNIT;
    ui_war_init(ui);
    ui->show_economy          = false;
    ui->report                = {};   /* M18 */
    ui->report.provider       = text_template_provider();
    ui->report.show_log       = true;
    ui->report.bubble_event   = ENGINE_NO_EVENT;
    for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind)
    {
        ui->show_command[kind] = false;
        ui->commands[kind] = {};
        ui->commands[kind].prepared_city = ENGINE_NO_CITY;
        ui->commands[kind].amount        = UI_MERCHANT_DEFAULT_AMOUNT;
    }
    ui->command_message[0]    = 0;
    ui->show_research         = false;
    ui->research              = {};
    ui->research.prepared_city = ENGINE_NO_CITY;
    ui->research.technique    = ENGINE_NO_TECHNIQUE;
    ui->show_merge            = false;
    ui->merge                 = {};
    ui->merge.prepared_city   = ENGINE_NO_CITY;
    ui->merge.absorber        = ENGINE_ANY_SLOT;
    ui->merge.absorbed        = ENGINE_ANY_SLOT;
    ui->show_techniques       = false;
    ui->show_sortie           = false;
    ui->show_transport        = false;
    ui->sortie                = {};
    ui->transport             = {};
    ui->sortie.prepared_city    = ENGINE_NO_CITY;
    ui->transport.prepared_city = ENGINE_NO_CITY;
    ui->show_unit             = false;
    ui->show_unit_menu        = false;
    ui->show_minimap          = false;
    ui->show_units            = false;   /* M11 */
    ui->units                 = {};
    ui->units.selected        = ENGINE_NO_UNIT;
    ui->units.supply_target   = ENGINE_NO_UNIT;
    ui->show_court            = false;
    ui->show_diplomacy        = false;
    ui->show_duel             = false;
    ui->show_debate           = false;
    ui->court                 = {};
    ui->court.officer = ui->court.target = ui->court.partner = ui->court.captive = ENGINE_NO_OFFICER;
    ui->court.post            = ENGINE_NO_ROW;
    ui->court.item            = ENGINE_NO_ROW;
    ui->show_reward           = false;
    ui->reward                = {};
    ui->diplomacy             = {};
    ui->diplomacy.force       = ENGINE_NO_FACTION;
    ui->diplomacy.second      = ENGINE_NO_FACTION;
    ui->diplomacy.city        = ENGINE_NO_CITY;
    ui->diplomacy.captive     = ENGINE_NO_OFFICER;
    ui->diplomacy.envoy       = ENGINE_NO_OFFICER;
    ui->diplomacy.gold        = 1000;
    ui->diplomacy.months      = 6;
    ui->show_ruler            = false;
    ui->show_bases            = false;
    ui->show_fidelity         = false;   /* M13 */
    ui->show_ai               = false;   /* M15 */
    ui->show_content          = false;   /* M16 */
    ui->ai.faction            = ENGINE_NO_FACTION;
    ui->ruler.viceroy         = ENGINE_NO_OFFICER;
    ui->ruler.adviser         = ENGINE_NO_OFFICER;
    ui->ruler.cities          = 0;
    ui->ruler.corps           = 1;
    ui->ruler.policy          = CORPS_POLICY_DELEGATE;
    ui->ruler.flags           = CORPS_FLAG_ALL;
    ui->game                  = {};   /* M9: the game screens; the game exe opens the title */
    ui->game.chosen           = ENGINE_SAVE_SLOTS;
    ui->game.city             = ENGINE_NO_CITY;
    ui->game.ancient          = SIM_NO_ANCIENT;
    ui->game.editor_officer   = ENGINE_NO_OFFICER;
    ui->game.trainee          = ENGINE_NO_OFFICER;
    ui->game.birth_year       = 170;
    ui->game.type             = OFFICER_TYPE_ALL_ROUND;
    ui->game.personality      = PERSONALITY_CALM;
    ui->game.editor_loyalty   = 80;
    ui->show_title = ui->show_scenarios = ui->show_rulers = ui->show_start_settings = false;
    ui->show_topbar = ui->show_history = ui->show_saveload = ui->show_options = false;
    ui->show_new_officer = ui->show_editor = ui->show_battles = ui->show_abilities = ui->show_info = false;
    ui->quit_requested = false;
    ui->hud_mode          = false;   /* the debug layout; a game start turns on the game mode */
    ui->show_debug_panels = false;
    ui->hud_tab           = UI_HUD_TAB_BASES;
    ui->context           = {};
    ui->ledger_city           = ENGINE_ANY_CITY;
    ui->ledger_resource       = ENGINE_ANY_RESOURCE;
    ui->text          = text;
    ui->pending_locale[0] = 0;
    ui->load_message[0] = 0;
    ui->names_message[0] = 0;
    ui_map_init(ui);
    ui_scene_init(ui);
}

internal void
ui_draw_usage(const char *name, u64 used, u64 size)
{
    f32 fraction = size ? (f32)((f64)used / (f64)size) : 0.0f;
    char label[64];
    snprintf(label, sizeof(label), "%.2f / %.2f MB", (f64)used / 1048576.0, (f64)size / 1048576.0);
    ImGui::Text("%s", name);
    ImGui::SameLine(140.0f);
    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), label);
}

internal void
ui_draw_menu_bar(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    if (ImGui::BeginMainMenuBar())
    {
        /* The debug panels keep English names; the game panels use the locale. */
        char label[128];
        if (ImGui::BeginMenu(ui_label(label, sizeof(label), ui, "menu.windows", "Windows")))
        {
            ImGui::MenuItem("Simulation", 0, &ui->show_sim);
            ImGui::MenuItem("Scene", 0, &ui->show_scene);
            ImGui::MenuItem("Hex map", 0, &ui->show_map);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.officers", "Officers"), 0, &ui->show_officers);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.cities", "Cities"), 0, &ui->show_cities);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.city", "City"), 0, &ui->show_city);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.develop", "Develop"), 0, &ui->show_develop);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.economy", "Economy"), 0, &ui->show_economy);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.techniques", "Techniques"), 0, &ui->show_techniques);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.unit", "Unit"), 0, &ui->show_unit);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.units", "Units"), 0, &ui->show_units);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.minimap", "MiniMap"), 0, &ui->show_minimap);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.court", "Court"), 0, &ui->show_court);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.diplomacy", "Diplomacy"), 0, &ui->show_diplomacy);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.ruler", "Ruler"), 0, &ui->show_ruler);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.bases", "Bases"), 0, &ui->show_bases);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.fidelity", "Fidelity"), 0, &ui->show_fidelity);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.ai", "AI"), 0, &ui->show_ai);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.content", "Content"), 0, &ui->show_content);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.title", "TitleMenu"), 0, &ui->show_title);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.abilities", "AbilitiesMenu"), 0, &ui->show_abilities);
            ImGui::MenuItem("Event ledger", 0, &ui->show_ledger);
            ImGui::MenuItem("Memory", 0, &ui->show_memory);
            ImGui::Separator();
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.settings", "Settings"), 0, &ui->show_settings);
            ImGui::MenuItem("ImGui demo", 0, &ui->show_demo);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        engine_date date = api->date(api->engine);
        ImGui::Text("turn %u  %u-%02u-%02u", api->turn(api->engine), date.year, date.month, date.day);
        ImGui::Separator();
        ImGui::Text("dll reloads %u", frame->dll_reload_count);
        ImGui::Separator();
        ImGui::Text("%.1f ms  %.0f fps", (f64)frame->dt_seconds * 1000.0, (f64)ImGui::GetIO().Framerate);
        ImGui::EndMainMenuBar();
    }
}

/* The date, the faction that acts, and the order of this turn. */
internal void
ui_draw_turn_status(ui_state *ui, engine_api *api)
{
    engine_date date = api->date(api->engine);
    ImGui::Text("Date: %u-%02u-%02u (%s), day %lld", date.year, date.month, date.day,
                calendar_season_name((calendar_season)date.season), (long long)date.days_since_start);
    u8 acting = api->whose_turn(api->engine);
    if (acting == ENGINE_NO_FACTION) ImGui::TextUnformatted("Acting: none (Advance turn moves the date)");
    else
    {
        ImGui::TextUnformatted("Acting:");
        ImGui::SameLine();
        ui_code_cell(ui, api, UI_CODE_FORCE, acting, ENGINE_NO_FACTION);
        ImGui::SameLine();
        ImGui::Text("(force %u), %u action points", acting, api->actions_left(api->engine, acting));
    }
    u32 factions = api->faction_count(api->engine);
    if (factions && ImGui::CollapsingHeader("Turn order"))
    {
        if (ImGui::BeginTable("turn_order", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("#");
            ImGui::TableSetupColumn("force");
            ImGui::TableSetupColumn("cities");
            ImGui::TableSetupColumn("officers");
            ImGui::TableSetupColumn("AP");
            ImGui::TableSetupColumn("player");
            ImGui::TableHeadersRow();
            for (u32 index = 0; index < factions; ++index)
            {
                engine_faction faction = api->faction_at(api->engine, index);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%u%s", index, faction.acted ? " done" : "");
                ImGui::TableNextColumn(); ui_code_cell(ui, api, UI_CODE_FORCE, faction.force_id, ENGINE_NO_FACTION);
                ImGui::TableNextColumn(); ImGui::Text("%u", faction.city_count);
                ImGui::TableNextColumn(); ImGui::Text("%u", faction.officer_count);
                ImGui::TableNextColumn(); ImGui::Text("%u", faction.action_points);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(faction.human ? "human" : "AI");
            }
            ImGui::EndTable();
        }
    }
}

internal void
ui_draw_simulation(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(380, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Simulation", &ui->show_sim))
    {
        ImGui::Text("Turn: %u", api->turn(api->engine));
        ImGui::Text("Entities alive: %u / %u", api->entities_alive(api->engine), api->entity_capacity(api->engine));
        ui_draw_turn_status(ui, api);
        ui_draw_outcome(ui, api);
        ImGui::Separator();
        /* "Advance turn" is the proceed button: the faction that acts ends its turn. */
        if (ImGui::Button("Advance turn"))
        {
            api->end_turn(api->engine);
        }
        ImGui::SameLine();
        if (ImGui::Button("Advance 12 turns"))
        {
            api->advance_turns(api->engine, 12);
        }
        ui_draw_load_sample_save(ui, api);
        ImGui::Spacing();
        engine_content_status content = api->content_status(api->engine);
        ImGui::Text("Content pack: %s (%u facilities, %u cities, %u rules)", content.loaded ? "loaded" : content.detail,
                    content.facility_count, content.city_count, content.rule_count);
        ImGui::TextWrapped("Mechanics: calendar, turn order, action points, develop (M1, M2). "
                           "Amounts marked 'assumed' in the ledger are not from the spec.");
    }
    ImGui::End();
}

/* The filters of the ledger: a city and a resource. A choice changes only
   the front end; the panel then asks the query ledger_filter. */
internal void
ui_draw_ledger_filters(ui_state *ui, engine_api *api)
{
    char label[128], preview[96];
    if (ui->ledger_city == ENGINE_ANY_CITY) snprintf(preview, sizeof(preview), "%s", text_get(ui->text, "ledger.any"));
    else                                    ui_city_label(ui, api, ui->ledger_city, preview, sizeof(preview));
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "ledger.city", "ledger_city"), preview))
    {
        snprintf(label, sizeof(label), "%s###lc_any", text_get(ui->text, "ledger.any"));
        if (ImGui::Selectable(label, ui->ledger_city == ENGINE_ANY_CITY)) ui->ledger_city = ENGINE_ANY_CITY;
        u32 count = api->city_count(api->engine);
        for (u32 index = 0; index < count; ++index)
        {
            engine_city city = api->city_at(api->engine, index);
            char name[96];
            ui_city_label(ui, api, city.city_id, name, sizeof(name));
            snprintf(label, sizeof(label), "%s###lc%u", name, city.city_id);
            if (ImGui::Selectable(label, ui->ledger_city == city.city_id)) ui->ledger_city = city.city_id;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ui->ledger_resource == ENGINE_ANY_RESOURCE) snprintf(preview, sizeof(preview), "%s", text_get(ui->text, "ledger.any"));
    else snprintf(preview, sizeof(preview), "%s", ui_text_with_key(ui, "resource", sim_resource_name((sim_resource)ui->ledger_resource)));
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "ledger.resource", "ledger_resource"), preview))
    {
        snprintf(label, sizeof(label), "%s###lr_any", text_get(ui->text, "ledger.any"));
        if (ImGui::Selectable(label, ui->ledger_resource == ENGINE_ANY_RESOURCE)) ui->ledger_resource = ENGINE_ANY_RESOURCE;
        for (u32 resource = SIM_RESOURCE_GOLD; resource < SIM_RESOURCE_COUNT; ++resource)
        {
            snprintf(label, sizeof(label), "%s###lr%u", ui_text_with_key(ui, "resource", sim_resource_name((sim_resource)resource)), resource);
            if (ImGui::Selectable(label, ui->ledger_resource == resource)) ui->ledger_resource = (u8)resource;
        }
        ImGui::EndCombo();
    }
}

/* What the detail field of an event holds (the comments of sim_event_kind
   in sim.h), so that the ledger can show a name in place of the number. */
enum ui_detail_kind
{
    UI_DETAIL_NUMBER,
    UI_DETAIL_UNIT,
    UI_DETAIL_WEAPON,
    UI_DETAIL_TECHNIQUE,
    UI_DETAIL_DISASTER,
    UI_DETAIL_OFFICER,
    UI_DETAIL_FORCE,
    UI_DETAIL_CITY,
};

internal ui_detail_kind
ui_event_detail_kind(sim_event_kind kind)
{
    switch (kind)
    {
        case SIM_EVENT_UNIT_SORTIED:        case SIM_EVENT_UNIT_SUPPLIES:      case SIM_EVENT_UNIT_GRAIN_TAKEN:
        case SIM_EVENT_UNIT_WEAPONS_TAKEN:  case SIM_EVENT_UNIT_MOVED:         case SIM_EVENT_UNIT_SOLDIERS_LOST:
        case SIM_EVENT_UNIT_WAITED:         case SIM_EVENT_UNIT_ENTERED_CITY:  case SIM_EVENT_UNIT_GOLD_RETURNED:
        case SIM_EVENT_UNIT_GRAIN_RETURNED: case SIM_EVENT_UNIT_RETREATED:     case SIM_EVENT_UNIT_SUPPLIED:
        case SIM_EVENT_UNIT_GRAIN_EATEN:    case SIM_EVENT_UNIT_DESERTED:      case SIM_EVENT_UNIT_DAMAGED:
        case SIM_EVENT_COUNTERATTACK:       case SIM_EVENT_CRITICAL_HIT:       case SIM_EVENT_TACTIC_USED:
        case SIM_EVENT_PLOT_USED:           case SIM_EVENT_MORALE_CHANGED:     case SIM_EVENT_UNIT_PUSHED:
        case SIM_EVENT_UNIT_STATE:          case SIM_EVENT_UNIT_DESTROYED:     case SIM_EVENT_UNIT_LOOT:
        case SIM_EVENT_AID_ARRIVED:         case SIM_EVENT_AID_LEFT:
        case SIM_EVENT_UNIT_ORDERED:        case SIM_EVENT_UNIT_ORDER_DONE:
            return UI_DETAIL_UNIT;
        case SIM_EVENT_WEAPONS_FREE: case SIM_EVENT_PRODUCTION_STARTED: case SIM_EVENT_WEAPONS_PRODUCED:
            return UI_DETAIL_WEAPON;
        case SIM_EVENT_TECHNIQUE_SPENT: case SIM_EVENT_RESEARCH_STARTED: case SIM_EVENT_TECHNIQUE_RESEARCHED:
            return UI_DETAIL_TECHNIQUE;
        case SIM_EVENT_DISASTER_STARTED: case SIM_EVENT_DISASTER_ENDED: case SIM_EVENT_GRAIN_LOST:
        case SIM_EVENT_GRAIN_HARVEST:    case SIM_EVENT_SOLDIERS_LOST:
            return UI_DETAIL_DISASTER;
        case SIM_EVENT_SWORN: case SIM_EVENT_MARRIED: case SIM_EVENT_DUEL_STARTED: case SIM_EVENT_DUEL_REFUSED:
        case SIM_EVENT_DEBATE_STARTED:
            return UI_DETAIL_OFFICER;
        case SIM_EVENT_FRIENDLINESS_CHANGED: case SIM_EVENT_RELATION_CHANGED: case SIM_EVENT_DIPLOMACY_FAILED:
        case SIM_EVENT_FACTION_SURRENDERED:  case SIM_EVENT_BASE_CAPTURED:    case SIM_EVENT_OFFICER_RECRUITED:
        case SIM_EVENT_OFFICER_LEFT:         case SIM_EVENT_FACTION_FOUNDED:
            return UI_DETAIL_FORCE;
        case SIM_EVENT_OFFICER_CREATED:
            return UI_DETAIL_CITY;
        default:
            return UI_DETAIL_NUMBER;
    }
}

/* The subject of an event as a name: the city or the officer behind the
   entity (subject_of); "-" for none; the number when raw numbers are on. */
internal void
ui_event_subject_text(ui_state *ui, engine_api *api, u32 entity, char *out, u32 out_size)
{
    if (!entity) { snprintf(out, out_size, "-"); return; }
    if (ui->show_raw) { snprintf(out, out_size, "%u", entity); return; }
    engine_subject subject = api->subject_of(api->engine, entity);
    if (subject.kind == ENGINE_SUBJECT_CITY)         ui_city_label(ui, api, (u8)subject.id, out, out_size);
    else if (subject.kind == ENGINE_SUBJECT_OFFICER) ui_officer_text(ui, api, subject.id, out, out_size);
    else                                             snprintf(out, out_size, "%u", entity);
}

/* The detail of an event as a name when the kind says what it is. */
internal void
ui_event_detail_text(ui_state *ui, engine_api *api, sim_event *ev, char *out, u32 out_size)
{
    if (!ui->show_raw)
    {
        switch (ui_event_detail_kind(ev->kind))
        {
            case UI_DETAIL_UNIT:
            {
                engine_unit unit = api->unit_at(api->engine, ev->detail);
                if (unit.id != ENGINE_NO_UNIT) { ui_unit_label(ui, api, &unit, out, out_size); return; }
                break;
            }
            case UI_DETAIL_WEAPON:
            {
                engine_weapon_kind kind = api->weapon_kind_at(api->engine, ev->detail);
                if (kind.id != ENGINE_NO_WEAPON) { snprintf(out, out_size, "%s", ui_text_with_key(ui, "weapon", kind.key)); return; }
                break;
            }
            case UI_DETAIL_TECHNIQUE:
            {
                engine_technique_kind kind = api->technique_kind_at(api->engine, ev->detail);
                if (kind.id != ENGINE_NO_TECHNIQUE) { snprintf(out, out_size, "%s", ui_text_with_key(ui, "technique", kind.key)); return; }
                break;
            }
            case UI_DETAIL_DISASTER:
            {
                engine_disaster_kind kind = api->disaster_kind_at(api->engine, ev->detail);
                if (kind.key[0]) { snprintf(out, out_size, "%s", ui_text_with_key(ui, "disaster", kind.key)); return; }
                break;
            }
            case UI_DETAIL_OFFICER: ui_officer_text(ui, api, (u16)ev->detail, out, out_size); return;
            case UI_DETAIL_FORCE:   ui_force_text(ui, api, (u8)ev->detail, out, out_size);    return;
            case UI_DETAIL_CITY:    ui_city_label(ui, api, (u8)ev->detail, out, out_size);    return;
            default: break;
        }
    }
    snprintf(out, out_size, "%u", ev->detail);
}

/* The rows of the ledger table: every event, or the events of the city and
   resource filters (ledger_filter). A source index is a place in that list. */
struct ui_ledger_rows
{
    b32                filtered;
    engine_ledger_view view;
};

internal u32
ui_ledger_event_index(ui_ledger_rows *rows, u32 source)
{
    return rows->filtered ? rows->view.indices[source] : source;
}

internal u8
ui_ledger_row_cells(ui_state *ui, engine_api *api, void *user, u32 source, ui_sort_key *cells)
{
    ui_ledger_rows *rows = (ui_ledger_rows *)user;
    u32 index = ui_ledger_event_index(rows, source);
    sim_event ev = api->ledger_at(api->engine, index);
    char text[UI_NAME_SIZE * 2];
    ui_key_number(cells + 0, index);
    ui_key_number(cells + 1, ev.turn);
    ui_key_text(cells + 2, sim_event_name(ev.kind));
    ui_event_subject_text(ui, api, ev.subject, text, sizeof(text));
    ui_key_text(cells + 3, text);
    ui_key_number(cells + 4, ev.delta);
    ui_key_number(cells + 5, ev.cause);
    ui_key_code(ui, api, cells + 6, UI_CODE_FORCE, ev.faction, ENGINE_NO_FACTION);
    ui_key_text(cells + 7, ev.assumed ? "assumed" : "");
    ui_event_detail_text(ui, api, &ev, text, sizeof(text));
    ui_key_text(cells + 8, text);
    return ENGINE_NO_FACTION;
}

internal void
ui_draw_ledger_row(ui_state *ui, engine_api *api, u32 index)
{
    sim_event ev = api->ledger_at(api->engine, index);
    char text[UI_NAME_SIZE * 2];
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%u", index);
    ImGui::TableNextColumn(); ImGui::Text("%u", ev.turn);
    ImGui::TableNextColumn(); ImGui::TextUnformatted(sim_event_name(ev.kind));
    ImGui::TableNextColumn();
    ui_event_subject_text(ui, api, ev.subject, text, sizeof(text));
    ImGui::TextUnformatted(text);
    ui_raw_tooltip(ui, ev.subject);
    ImGui::TableNextColumn(); ImGui::Text("%d", ev.delta);
    ImGui::TableNextColumn(); ImGui::Text("%u", ev.cause);
    ImGui::TableNextColumn(); ui_code_cell(ui, api, UI_CODE_FORCE, ev.faction, ENGINE_NO_FACTION);
    ImGui::TableNextColumn(); ImGui::TextUnformatted(ev.assumed ? "assumed" : "");
    ImGui::TableNextColumn();
    ui_event_detail_text(ui, api, &ev, text, sizeof(text));
    ImGui::TextUnformatted(text);
    ui_raw_tooltip(ui, ev.detail);
}

internal void
ui_draw_ledger(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(760, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Event ledger", &ui->show_ledger))
    {
        u32 count = api->ledger_count(api->engine);
        ImGui::Text("%u events", count);
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
        {
            api->ledger_clear(api->engine);
            count = 0;
        }
        ImGui::SameLine();
        ui_draw_ledger_filters(ui, api);
        ui_ledger_rows rows = {};
        rows.filtered = ui->ledger_city != ENGINE_ANY_CITY || ui->ledger_resource != ENGINE_ANY_RESOURCE;
        if (rows.filtered)
        {
            rows.view = api->ledger_filter(api->engine, ui->ledger_city, ui->ledger_resource);
            count     = rows.view.count;
        }
        ui_table_source source = {};
        source.source_count = count;
        source.column_count = 9;
        source.only_force   = ENGINE_NO_FACTION;
        source.row          = ui_ledger_row_cells;
        source.user         = &rows;
        source.rows         = ui->ledger_rows;
        source.keys         = ui->ledger_keys;
        source.capacity     = ArrayCount(ui->ledger_rows);
        ui_table_view_update(ui, api, &ui->ledger_view, &source);
        ui_table_filter_box(ui, &ui->ledger_view, "filter_ledger");
        ImGui::SameLine();
        ui_raw_toggle(ui);
        if (ImGui::BeginTable("ledger", 9, UI_TABLE_FLAGS))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("turn", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("event");
            ImGui::TableSetupColumn("subject", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("delta", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("cause", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("faction", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("assumed", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableHeadersRow();
            ui_table_read_sort(&ui->ledger_view);
            ImGuiListClipper clipper;
            clipper.Begin((int)ui->ledger_view.count);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    ui_draw_ledger_row(ui, api, ui_ledger_event_index(&rows, ui->ledger_rows[row]));
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

internal void
ui_draw_memory(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    ImGui::SetNextWindowSize(ImVec2(420, 160), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Memory", &ui->show_memory))
    {
        engine_memory_stats stats = api->memory(api->engine);
        ui_draw_usage("permanent", stats.permanent_used, stats.permanent_size);
        ui_draw_usage("transient", stats.transient_used, stats.transient_size);
        ui_draw_usage("frame", frame->frame_arena_used, frame->frame_arena_size);
    }
    ImGui::End();
}

internal void
ui_draw(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    ui_apply_pending_locale(ui);
    ui->portraits        = frame->portraits;
    ui->portrait_texture = frame->portrait_texture;
    ui->face_texture     = frame->face_texture;
    if (!ui->show_map) ui->map.focused = false;   /* a closed panel takes no keys */
    /* The game mode hides the debug menu bar and the debug tools; F12 shows them. */
    if (ui->hud_mode && ImGui::IsKeyPressed(ImGuiKey_F12, false)) ui->show_debug_panels = !ui->show_debug_panels;
    bool tools = !ui->hud_mode || ui->show_debug_panels;
    bool in_game = !ui->show_title && !ui->show_scenarios && !ui->show_rulers && !ui->show_start_settings;
    ui_feel_update(ui, api, frame->dt_seconds, in_game);   /* M17: the rider, the load transition and the turn start */
    bool hud_ready = ui->hud_mode && in_game && !ui_feel_in_transition(ui);
    if (in_game) ui_report_update(ui, api, frame->dt_seconds);   /* M18: the log and the feedback read the new events */
    if (in_game) ui_round_update(ui, api, frame->dt_seconds);    /* M20: the next step of the show of the AI round */
    if (in_game) ui_war_update(ui, api, frame->dt_seconds);      /* the walk, the strikes and the hits */
    if (tools) ui_draw_menu_bar(ui, api, frame);
    if (tools && ui->show_demo)   ImGui::ShowDemoWindow(&ui->show_demo);
    if (tools && ui->show_sim)    ui_draw_simulation(ui, api);
    if (tools && ui->show_scene)  ui_draw_scene_panel(ui, api);
    if (hud_ready)                ui_draw_hud(ui, api);
    bool panels_hidden = ui->hud_mode && ui_hud_panels_hidden(ui);   /* M22, screen-flow.md, rule 3 */
    if (hud_ready && !panels_hidden) ui_draw_log(ui, api);   /* M18: the message log at the bottom left */
    if (ui->show_map)    ui_draw_map(ui, api);
    if (ui->show_officers) ui_draw_officers(ui, api);
    if (ui->show_cities) ui_draw_cities(ui, api);
    if (tools && ui->show_ledger) ui_draw_ledger(ui, api);
    if (tools && ui->show_memory) ui_draw_memory(ui, api, frame);
    if (ui->show_settings) ui_draw_settings(ui, api);
    /* M19, ui/city-commands.md: in the game mode the city panel hides while
       a command dialog of the city is open. */
    bool command_open = ui->show_develop || ui->show_picker;
    for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind) command_open = command_open || ui->show_command[kind];
    if (ui->show_city && !(ui->hud_mode && command_open)) ui_draw_city(ui, api);
    if (ui->show_develop) ui_draw_develop(ui, api);
    if (ui->show_economy) ui_draw_economy(ui, api);
    if (ui->show_research)   ui_draw_research(ui, api);
    if (ui->show_merge)      ui_draw_merge(ui, api);
    if (ui->show_techniques) ui_draw_techniques(ui, api);
    if (ui->show_sortie)     ui_draw_march_dialog(ui, api, false);
    if (ui->show_transport)  ui_draw_march_dialog(ui, api, true);
    if (ui->show_unit)       ui_draw_unit_panel(ui, api);
    if (ui->show_unit_menu)  ui_draw_unit_menu(ui, api);
    if (ui->units.target_kind != UI_TARGET_NONE) ui_draw_unit_target_menu(ui, api);   /* G2-052, G2-058 */
    if (ui->show_minimap && !ui_feel_in_transition(ui) && !panels_hidden) ui_draw_minimap(ui, api);
    if (ui->show_units)      ui_draw_units_panel(ui, api);   /* M11 */
    ui_draw_hex_info_box(ui, api);                           /* M11: opens after a pick in the scene */
    if (ui->show_actions)    ui_draw_actions(ui, api);
    if (ui->show_court)      ui_draw_court(ui, api);
    if (ui->show_reward)     ui_draw_reward(ui, api);   /* 褒賞 (G2-040) */
    if (ui->show_diplomacy)  ui_draw_diplomacy(ui, api);
    ui_draw_people_dialogs(ui, api);   /* 移動, 召喚, 官職, 流放, the plots (clone gap report 2, group 3) */
    ui_draw_mini_games(ui, api);   /* the duel and debate screens open when one runs */
    if (ui->show_ruler)      ui_draw_ruler(ui, api);
    if (ui->show_bases)      ui_draw_bases(ui, api);
    if (ui->show_fidelity)   ui_draw_fidelity(ui, api);   /* M13 */
    if (ui->show_ai)         ui_draw_ai(ui, api);         /* M15 */
    if (ui->show_content)    ui_draw_content(ui, api);    /* M16 */
    ui_draw_event_dialog(ui, api);   /* opens while an event waits for a choice */
    ui_draw_game_screens(ui, api);   /* M9: the title, the scenario list, the top bar and the other game screens */
    ui_draw_messages(ui, api);       /* M17: the reports at the start of a turn */
    ui_draw_load_transition(ui);     /* M17: on top of everything while a save loads */
    for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind)
    {
        if (ui->show_command[kind]) ui_draw_city_command(ui, api, kind);
    }
    if (ui->show_picker) ui_draw_picker(ui, api);      /* M19: over the dialog that opened it */
    if (hud_ready)       ui_draw_placement(ui, api);   /* M19: the panel of a 開發 that waits for its slot */
    if (ui->hud_mode) ui_draw_hud_menus(ui, api);   /* the map menu and the system menu */
    if (ui->hud_mode) ui_draw_info_screens(ui, api);   /* M21: the screens of the information menu */
    if (ui->hud_mode && ui->show_political) ui_draw_political(ui, api);   /* M21: 中地圖 */
    if (ui->hud_mode && ui->context.kind == UI_CONTEXT_UNIT_POPUP) ui_draw_unit_popup(ui, api);   /* M22 */
    ui_context_update(ui, api);                     /* a right click outside a dialog closes it */

    /* The scene goes last: by now ImGui knows which windows want the mouse,
       and the panels above may have changed the camera or the selection. */
    if (ui->scene.enabled && (frame->render || ui->scene.headless))
    {
        if (!ui->scene.initialized) ui_scene_reset_camera(ui, api);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ui_scene_handle_mouse(ui, api, frame->render ? frame->render->viewport_width : display.x,
                              frame->render ? frame->render->viewport_height : display.y);
        bool screen_open = ui->info.screen != 0 || ui->show_political;   /* M21: a screen of the information menu covers the map */
        if (hud_ready && !ui_feel_messages_open(ui) && !ui->round.playing && !ui->round.confirm && !screen_open) ui_hud_scene_clicks(ui, api);   /* the dialogs at the clicked city, unit or map */
        if (hud_ready && !screen_open) ui_draw_hover_panel(ui, api);   /* M21: the hover panel of a city or a unit */
        ui_scene_handle_keys(ui, frame->dt_seconds);
        ui_follow_selection(ui, api);   /* M11: a picked city or unit moves the camera */
        ui_draw_scene_labels(ui, api);  /* M11: the plates of the bases and the units, the slots */
        if (ui->show_ai) ui_draw_ai_lines(ui, api);   /* M15: each unit of the AI view to its goal */
    }
    if (hud_ready) ui_draw_feedback(ui, api);   /* M18: the speech bubble and the values over the base of a command */
    if (in_game)   ui_draw_war(ui, api);        /* the strikes and the values over the units */
    ui_draw_status_bar(ui, api);        /* M11: the terrain, the coordinates and what can stand on the hex under the mouse */
    ui_scene_push(ui, api, frame);
}
