/* M16: the content of the player's own copy (ROADMAP.md, M16).

   The player can name the folder of an install of the original. The engine
   reads two kinds of file from media\scenario of that folder, because the
   spec gives their layout (formats/scenario-file.md):
   - Scenario.s11, the name index: it gives the names of the cities, gates
     and ports, through the same command as load_name_index;
   - SCEN000.s11 to SCEN013.s11: the scenario list of a new game, with the
     number, the start date and the title of each file.
   The spec has no layout of the message, portrait and music files
   (formats/binary-identity.md: "Asset formats are out of scope"). For the
   messages the engine counts the files S11MSG00..03.s11 of the install. It
   does not decode them. content_source reports the source of each asset. */

#define ENGINE_INSTALL_SCENARIO_FOLDER "media/scenario"
#define ENGINE_INSTALL_MESSAGE_FOLDER  "media/msg"
#define ENGINE_INSTALL_MESSAGE_FILES   4     /* S11MSG00..03.s11 (formats/binary-identity.md) */
#define ENGINE_INSTALL_PATH_RESERVE    40    /* the longest file name under the folder, with its NUL */

/* True if the platform reader gives the file. The reader has no other test
   of a file, so a file larger than the free transient memory counts as
   absent. The memory goes back at once. */
internal b32
engine_install_file_exists(engine *e, const char *path)
{
    if (!e->read_entire_file) return false;
    u64 mark = e->transient->used;
    b32 exists = e->read_entire_file(path, e->transient).contents != 0;
    e->transient->used = mark;
    return exists;
}

/* Reads SCEN<number>.s11 of the folder. Returns true and the row if the file
   loads as a scenario. */
internal b32
engine_read_install_scenario(engine *e, const char *folder, u32 number, engine_install_scenario *out)
{
    *out = {};
    snprintf(out->file, sizeof(out->file), "%s/" ENGINE_INSTALL_SCENARIO_FOLDER "/SCEN%03u.s11", folder, number);
    engine_load_result result;
    read_file_result file;
    if (!engine_read_file_to_scratch(e, out->file, &file, &result)) return false;
    scenario_file scenario;
    if (scenario_load(&e->scratch, (const u8 *)file.contents, file.size, &scenario) != SCENARIO_LOAD_OK) return false;
    out->number = scenario.header.scenario_number;
    out->year   = scenario.header.start_year;
    out->month  = scenario.header.start_month;
    memcpy(out->title, scenario.header.scenario_title, sizeof(out->title));
    return true;
}

internal void
engine_clear_install(engine *e)
{
    e->install_folder[0]      = 0;
    e->install_scenario_count = 0;
    e->install_message_files  = 0;
}

internal engine_install_result
engine_api_set_install_folder(void *opaque, const char *folder)
{
    engine *e = (engine *)opaque;
    engine_install_result result = {};
    result.detail = "ok";
    if (!folder || !folder[0])
    {
        engine_clear_install(e);
        result.detail = "the install folder is cleared";
        return result;
    }
    if (strlen(folder) + ENGINE_INSTALL_PATH_RESERVE >= ENGINE_PATH_SIZE)
    {
        result.detail = "the folder path is too long";
        return result;
    }

    engine_install_scenario scenarios[ENGINE_INSTALL_SCENARIO_CAPACITY];
    u32 scenario_count = 0;
    for (u32 number = 0; number < ENGINE_INSTALL_SCENARIO_CAPACITY; ++number)
    {
        if (engine_read_install_scenario(e, folder, number, scenarios + scenario_count)) scenario_count++;
    }

    char names_path[ENGINE_PATH_SIZE];
    snprintf(names_path, sizeof(names_path), "%s/" ENGINE_INSTALL_SCENARIO_FOLDER "/Scenario.s11", folder);
    engine_load_result names = engine_api_load_name_index(e, names_path);
    b32 names_loaded = names.status == ENGINE_LOAD_OK;
    if (!names_loaded && scenario_count == 0)
    {
        result.detail = "the folder has no Scenario.s11 and no SCEN000..013.s11 under media/scenario";
        return result;
    }

    snprintf(e->install_folder, sizeof(e->install_folder), "%s", folder);
    memcpy(e->install_scenarios, scenarios, sizeof(engine_install_scenario) * scenario_count);
    e->install_scenario_count = scenario_count;
    if (names_loaded) e->names_source = ENGINE_SOURCE_INSTALL;

    e->install_message_files = 0;
    for (u32 index = 0; index < ENGINE_INSTALL_MESSAGE_FILES; ++index)
    {
        char path[ENGINE_PATH_SIZE];
        snprintf(path, sizeof(path), "%s/" ENGINE_INSTALL_MESSAGE_FOLDER "/S11MSG%02u.s11", folder, index);
        if (engine_install_file_exists(e, path)) e->install_message_files++;
    }

    result.found          = true;
    result.names_loaded   = names_loaded;
    result.scenario_count = scenario_count;
    result.command_id     = names_loaded ? names.command_id : 0;
    return result;
}

internal const char *
engine_api_install_folder(void *opaque)
{
    return ((engine *)opaque)->install_folder;
}

internal engine_content_source
engine_api_content_source(void *opaque, u8 asset)
{
    engine *e = (engine *)opaque;
    engine_content_source result = {};
    result.asset  = asset;
    result.source = ENGINE_SOURCE_NONE;
    switch (asset)
    {
        case ENGINE_ASSET_CITY_NAMES:
        {
            result.source = e->names_source;
            result.count  = e->names_source == ENGINE_SOURCE_NONE ? 0 : SCENARIO_CITY_COUNT;
            if (e->names_source == ENGINE_SOURCE_INSTALL) result.install_files = 1;
            snprintf(result.path, sizeof(result.path), "%s", e->names_path);
            result.detail = e->names_source == ENGINE_SOURCE_INSTALL ? "Scenario.s11 of the install" :
                            e->names_source == ENGINE_SOURCE_PACK    ? "the name index that the game loaded" :
                                                                       "no name index is loaded";
        } break;
        case ENGINE_ASSET_SCENARIOS:
        {
            if (e->install_scenario_count > 0)
            {
                result.source        = ENGINE_SOURCE_INSTALL;
                result.count         = e->install_scenario_count;
                result.install_files = e->install_scenario_count;
                result.detail        = "SCEN000..013.s11 of the install";
            }
            else
            {
                result.source = ENGINE_SOURCE_PACK;
                result.count  = e->scenario_count;
                result.detail = "scenarios.tsv of the content pack";
            }
        } break;
        case ENGINE_ASSET_MESSAGES:
        {
            result.source        = ENGINE_SOURCE_PACK;
            result.install_files = e->install_message_files;
            result.detail        = e->install_message_files > 0
                                 ? "text of the pack: the spec has no layout of the message files of the install"
                                 : "text of the pack";
        } break;
        case ENGINE_ASSET_PORTRAITS:
        {
            result.detail = "none: the spec has no layout of the portrait files";
        } break;
        case ENGINE_ASSET_MUSIC:
        {
            result.detail = "none: the spec has no layout of the music files";
        } break;
        default:
        {
            result.asset  = ENGINE_ASSET_KIND_COUNT;
            result.detail = "no such asset";
        } break;
    }
    return result;
}

internal u32
engine_api_install_scenario_count(void *opaque)
{
    return ((engine *)opaque)->install_scenario_count;
}

internal engine_install_scenario
engine_api_install_scenario_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    engine_install_scenario result = {};
    result.number = ENGINE_NO_ROW;
    if (index < e->install_scenario_count) result = e->install_scenarios[index];
    return result;
}

internal void
engine_bind_content_api(engine_api *api)
{
    api->set_install_folder     = engine_api_set_install_folder;
    api->install_folder         = engine_api_install_folder;
    api->content_source         = engine_api_content_source;
    api->install_scenario_count = engine_api_install_scenario_count;
    api->install_scenario_at    = engine_api_install_scenario_at;
}
