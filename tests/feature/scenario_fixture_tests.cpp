/* Feature tests of the scenario loader and the name-index loader on the real
   files in spec-src/spec/fixtures/scenarios/: SCEN002.s11 and Scenario.s11.

   These tests call scenario_load, name_index_load and save_load directly.
   engine_api has no command that loads a scenario yet, so the loaders are
   the only way in. The name index through engine_api has its own tests in
   name_index_tests.cpp.

   Expected values come from spec-src/spec/formats/scenario-file.md: the
   container, the layout, the owner list of SCEN002, the Hanzhong values,
   the name-index tables and the alignment of SCEN002 against the save that
   started from it. sample.s11 was saved from SCEN002 (fixtures README). */

#define SCEN002_FIXTURE      "scenarios/SCEN002.s11"
#define NAME_INDEX_FIXTURE   "scenarios/Scenario.s11"
#define SAMPLE_SAVE_FIXTURE  "saves/sample.s11"

/* scenario-file.md, Container, Layout and Officer record. */
#define SPEC_SCENARIO_NAMED_OFFICERS   850
#define SPEC_SCEN002_NUMBER            2
#define SPEC_SCEN002_START_YEAR        194
#define SPEC_SCEN002_START_MONTH       6
#define SPEC_SCEN002_START_PERIOD      1
#define SPEC_SCEN002_TITLE_BYTES       8     /* four Big5 characters */
#define SPEC_FORCE_RELATION_MAXIMUM    100
#define SPEC_HANZHONG_CITY_ID          36
#define SPEC_CHENLIU_CITY_ID           12
#define SPEC_LIU_DAI_OFFICER_ID        631   /* 武將一覽 of the original at the start of 194: loyalty 98 */
#define SPEC_GUO_JIA_OFFICER_ID        63    /* the byte is 120; the screen shows 100 */
#define SPEC_ZHANG_LU_FORCE_ID         9
#define SPEC_ZHANG_LU_OFFICER_ID       442   /* save-file.md: officer 442 rules force 9 from city 36 */
#define SPEC_BIG5_CHARACTER_BYTES      2u

/* scenario-file.md, City record +8: the owner of each city that the spec
   names for SCEN002. The spec does not name the owners of Anding (19),
   Jianye (22) and Wu (23), so these tests do not check them. */
struct spec_city_owner
{
    u8 city_id;
    u8 owner_force_id;
};

global_variable spec_city_owner spec_scen002_city_owners[] =
{
    {  0, 12  },   /* Xiangping */
    {  1, 11  },   /* Beiping */
    {  2, 11  },   /* Ji */
    {  3, 7   },   /* Nanpi */
    {  4, 7   },   /* Pingyuan */
    {  5, 15  },   /* Jinyang */
    {  6, 7   },   /* Ye */
    {  7, 13  },   /* Beihai */
    {  8, 14  },   /* Xiapi */
    {  9, 1   },   /* Xiaopei */
    { 10, 8   },   /* Shouchun */
    { 11, 10  },   /* Puyang */
    { 12, 0   },   /* Chenliu */
    { 13, 255 },   /* Xuchang */
    { 14, 8   },   /* Runan */
    { 15, 255 },   /* Luoyang */
    { 16, 255 },   /* Wan */
    { 17, 5   },   /* Chang'an */
    { 18, 255 },   /* Shangyong */
    { 20, 3   },   /* Tianshui */
    { 21, 3   },   /* Wuwei */
    { 24, 16  },   /* Kuaiji */
    { 25, 2   },   /* Lujiang */
    { 26, 255 },   /* Chaisang */
    { 27, 6   },   /* Jiangxia */
    { 28, 255 },   /* Xinye */
    { 29, 6   },   /* Xiangyang */
    { 30, 6   },   /* Jiangling */
    { 31, 255 },   /* Changsha */
    { 32, 255 },   /* Wuling */
    { 33, 255 },   /* Guiyang */
    { 34, 255 },   /* Lingling */
    { 35, 255 },   /* Yong'an */
    { 36, 9   },   /* Hanzhong */
    { 37, 4   },   /* Zitong */
    { 38, 4   },   /* Jiangzhou */
    { 39, 4   },   /* Chengdu */
    { 40, 255 },   /* Jianning */
    { 41, 255 },   /* Yunnan */
};

/* scenario-file.md, Scenario.s11: the number of Big5 characters in the name
   of each city id 0..41. Big5 uses two bytes for each character, so this
   gives the length of each name without a character table (Q-013). */
global_variable u8 spec_city_name_characters[SCENARIO_CITY_COUNT] =
{
    2, 2, 1, 2, 2, 2, 1, 2, 2, 2,   /* Xiangping .. Xiaopei */
    2, 2, 2, 2, 2, 2, 1, 2, 2, 2,   /* Shouchun .. Anding */
    2, 2, 2, 1, 2, 2, 2, 2, 2, 2,   /* Tianshui .. Xiangyang */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,   /* Jiangling .. Chengdu */
    2, 2                            /* Jianning, Yunnan */
};

/* The gates 42..50 and the first and last port, with their character counts. */
struct spec_gate_name
{
    u8 id;
    u8 characters;
};

global_variable spec_gate_name spec_gate_names[] =
{
    { 42, 2 },   /* Huguan */
    { 43, 3 },   /* Hulao Pass */
    { 44, 2 },   /* Tong Pass */
    { 45, 3 },   /* Hangu Pass */
    { 46, 2 },   /* Wu Pass */
    { 47, 3 },   /* Yangping Pass */
    { 48, 2 },   /* Jiange */
    { 49, 3 },   /* Jiameng Pass */
    { 50, 3 },   /* Fushui Pass */
    { 51, 3 },   /* Anping Port, the first port */
    { 86, 3 },   /* Wuxian Port, the last port */
};

internal u32
fixture_name_length(const u8 *bytes, u32 size)
{
    u32 length = 0;
    while (length < size && bytes[length]) length++;
    return length;
}

/* Each lead byte of a Big5 character has its high bit set. */
internal b32
fixture_name_is_big5(const u8 *bytes, u32 length)
{
    if (length == 0 || length % SPEC_BIG5_CHARACTER_BYTES) return false;
    for (u32 index = 0; index < length; index += SPEC_BIG5_CHARACTER_BYTES)
    {
        if (bytes[index] < 0x81) return false;
    }
    return true;
}

internal void
feature_name_index_fixture_tests(test_counts *counts, memory_arena *arena)
{
    char path[MAX_PATH];
    test_fixture_path(path, MAX_PATH, NAME_INDEX_FIXTURE);
    test_file file = test_read_file(path, arena);
    TEST_CHECK(counts, file.contents != 0, "Scenario.s11 is in the fixtures");
    if (!file.contents) return;
    TEST_CHECK(counts, file.size == NAME_INDEX_FILE_SIZE, "Scenario.s11 has 47,928 bytes");

    name_index names;
    scenario_load_status status = name_index_load(file.contents, file.size, &names);
    TEST_CHECK(counts, status == SCENARIO_LOAD_OK, "Scenario.s11 loads as a name index");
    if (status != SCENARIO_LOAD_OK) return;
    TEST_CHECK(counts, names.file_kind == SCENARIO_FILE_KIND_NAME_INDEX, "the name index has file kind 0x18");

    b32 ids_in_order = true, lengths_agree = true, names_big5 = true, names_unique = true;
    for (u32 city = 0; city < SCENARIO_CITY_COUNT; ++city)
    {
        name_index_city *entry = names.cities + city;
        u32 length = fixture_name_length(entry->name, NAME_INDEX_CITY_NAME_SIZE);
        if ((u32)entry->id != city) ids_in_order = false;
        if (length != SPEC_BIG5_CHARACTER_BYTES * spec_city_name_characters[city]) lengths_agree = false;
        if (!fixture_name_is_big5(entry->name, length)) names_big5 = false;
        for (u32 other = 0; other < city; ++other)
        {
            if (memcmp(entry->name, names.cities[other].name, NAME_INDEX_CITY_NAME_SIZE) == 0) names_unique = false;
        }
    }
    TEST_CHECK(counts, ids_in_order, "name index cities are ids 0..41 in order");
    TEST_CHECK(counts, lengths_agree, "each city name has two bytes for each character of its spec name");
    TEST_CHECK(counts, names_big5, "each city name is Big5 lead and trail bytes");
    TEST_CHECK(counts, names_unique, "no two cities have the same name");
    /* The province index and the adjacency list are unknown: the offsets of
       the spec table and the examples of the spec do not agree on this file
       (QUESTIONS.md, Q-017). These tests do not assert them. */

    b32 gate_ids_in_order = true, gates_named = true;
    for (u32 gate = 0; gate < NAME_INDEX_GATE_COUNT; ++gate)
    {
        name_index_gate *entry = names.gates_and_ports + gate;
        u32 length = fixture_name_length(entry->name, NAME_INDEX_GATE_ENTRY_SIZE);
        if ((u32)entry->id != NAME_INDEX_FIRST_GATE_ID + gate) gate_ids_in_order = false;
        if (!fixture_name_is_big5(entry->name, length)) gates_named = false;
    }
    TEST_CHECK(counts, gate_ids_in_order, "gates and ports are ids 42..86 in order");
    TEST_CHECK(counts, gates_named, "each gate and port has a Big5 name");

    b32 gate_lengths_agree = true;
    for (u32 index = 0; index < ArrayCount(spec_gate_names); ++index)
    {
        name_index_gate *entry = names.gates_and_ports + (spec_gate_names[index].id - NAME_INDEX_FIRST_GATE_ID);
        u32 length = fixture_name_length(entry->name, NAME_INDEX_GATE_ENTRY_SIZE);
        if (length != SPEC_BIG5_CHARACTER_BYTES * spec_gate_names[index].characters) gate_lengths_agree = false;
    }
    TEST_CHECK(counts, gate_lengths_agree, "the gates 42..50 and the ports 51 and 86 have the name lengths of the spec");

    scenario_file not_a_scenario;
    TEST_CHECK(counts, scenario_load(arena, file.contents, file.size, &not_a_scenario) != SCENARIO_LOAD_OK,
               "Scenario.s11 is not a scenario");
}

internal void
feature_scen002_header_tests(test_counts *counts, scenario_header *header)
{
    u8 marker[4] = { 0x00, 0x00, 0xFE, 0xFF };
    TEST_CHECK(counts, memcmp(header->constant_marker, marker, sizeof(marker)) == 0, "SCEN002 starts with 00 00 FE FF");
    TEST_CHECK(counts, header->file_kind == SCENARIO_FILE_KIND_SCENARIO, "SCEN002 has file kind 0x16");
    TEST_CHECK(counts, header->constant_at_0x018 == 1 && header->constant_at_0x01c == 2, "SCEN002 has 1 at 0x018 and 2 at 0x01C");
    TEST_CHECK(counts, header->scenario_number == SPEC_SCEN002_NUMBER, "SCEN002 is scenario number 2");
    TEST_CHECK(counts, header->start_year == SPEC_SCEN002_START_YEAR && header->start_month == SPEC_SCEN002_START_MONTH &&
                       header->start_ten_day_period == SPEC_SCEN002_START_PERIOD,
               "SCEN002 starts in year 194, month 6, period 1");
    u32 title_length = fixture_name_length(header->scenario_title, SCENARIO_TITLE_MAX_SIZE);
    TEST_CHECK(counts, title_length == SPEC_SCEN002_TITLE_BYTES && fixture_name_is_big5(header->scenario_title, title_length),
               "the SCEN002 title is four Big5 characters");
}

/* The officer table of scenario-file.md and the observed ranges of the
   shared layout (save-file.md, Officer record). */
internal void
feature_scen002_officer_tests(test_counts *counts, scenario_file *scenario)
{
    TEST_CHECK(counts, scenario->officer_slot_count == SCENARIO_OFFICER_SLOTS, "SCEN002 has 850 officer slots");
    u32 named = 0;
    b32 ids_in_order = true, statuses_known = true, compatibility_in_range = true, skills_in_range = true;
    b32 locations_in_range = true, forces_in_range = true;
    for (u32 slot = 0; slot < scenario->officer_slot_count; ++slot)
    {
        officer_record *officer = scenario->officers + slot;
        if (!officer_slot_is_empty(officer)) named++;
        if ((u32)officer->id != slot) ids_in_order = false;
        if (officer->status > OFFICER_STATUS_DEAD && officer->status != OFFICER_STATUS_EMPTY_SLOT) statuses_known = false;
        if (officer->compatibility > 149) compatibility_in_range = false;
        if (officer->skill_number > 99 && officer->skill_number != 255) skills_in_range = false;
        if (officer->base_location > 86 && officer->base_location != 0xFFFF) locations_in_range = false;
        if (officer->force_id >= SCENARIO_FORCE_COUNT && officer->force_id != 255) forces_in_range = false;
    }
    TEST_CHECK(counts, named == SPEC_SCENARIO_NAMED_OFFICERS, "SCEN002 has 850 named officers");
    TEST_CHECK(counts, ids_in_order, "SCEN002 officers are ids 0..849 in slot order");
    TEST_CHECK(counts, statuses_known, "SCEN002 officer status is 0..8 or 255");
    TEST_CHECK(counts, compatibility_in_range, "SCEN002 officer compatibility is 0..149");
    TEST_CHECK(counts, skills_in_range, "SCEN002 officer skill is 0..99 or 255");
    TEST_CHECK(counts, locations_in_range, "SCEN002 officer base location is a city, gate or port, or none");
    TEST_CHECK(counts, forces_in_range, "SCEN002 officer force is a force record, or none");
    TEST_CHECK(counts, !scenario->officers[0].has_save_fields, "a scenario officer has no save-only fields");

    officer_record *ruler = scenario->officers + SPEC_ZHANG_LU_OFFICER_ID;
    TEST_CHECK(counts, ruler->status == OFFICER_STATUS_RULER && ruler->force_id == SPEC_ZHANG_LU_FORCE_ID &&
                       ruler->base_location == SPEC_HANZHONG_CITY_ID,
               "SCEN002 officer 442 is the ruler of force 9 in Hanzhong (36)");

    /* scenario-file.md: bytes +0..+147 are the save record. save-file.md +104 is the loyalty (confirmed 2026-10-22). */
    officer_record *liu_dai = scenario->officers + SPEC_LIU_DAI_OFFICER_ID;
    officer_record *guo_jia = scenario->officers + SPEC_GUO_JIA_OFFICER_ID;
    TEST_CHECK(counts, liu_dai->has_loyalty && liu_dai->loyalty == 98 && guo_jia->loyalty == 120,
               "a scenario officer has the loyalty byte +104: 98 for Liu Dai, 120 for Guo Jia");
}

/* scenario-file.md, Force record. */
internal void
feature_scen002_force_tests(test_counts *counts, scenario_file *scenario)
{
    TEST_CHECK(counts, scenario->force_count == SCENARIO_FORCE_COUNT, "SCEN002 has 47 force records");
    b32 relations_in_range = true, own_relation_full = true;
    for (u32 force = 0; force < scenario->force_count; ++force)
    {
        force_record *record = scenario->forces + force;
        for (u32 other = 0; other < FORCE_RELATION_COUNT; ++other)
        {
            if (record->relations[other] > SPEC_FORCE_RELATION_MAXIMUM) relations_in_range = false;
        }
        if (force < FORCE_RELATION_COUNT && record->relations[force] != SPEC_FORCE_RELATION_MAXIMUM) own_relation_full = false;
    }
    /* The spec lists the values seen as 20..100; SCEN002 also has 0, 10 and 80 (Q-018). */
    TEST_CHECK(counts, relations_in_range, "each force relation is 0..100");
    TEST_CHECK(counts, own_relation_full, "the relation of each force toward itself is 100");
    TEST_CHECK(counts, scenario->forces[SPEC_ZHANG_LU_FORCE_ID].ruler_officer_id == SPEC_ZHANG_LU_OFFICER_ID,
               "the ruler of force 9 is officer 442");

    /* The spec names the ruler and the advisor of forces 0, 1, 9 and 10. */
    u8 named_forces[] = { 0, 1, 9, 10 };
    b32 rulers_agree = true, advisors_agree = true;
    for (u32 index = 0; index < ArrayCount(named_forces); ++index)
    {
        force_record *record = scenario->forces + named_forces[index];
        if (record->ruler_officer_id >= SCENARIO_OFFICER_SLOTS || record->advisor_officer_id >= SCENARIO_OFFICER_SLOTS)
        {
            rulers_agree = advisors_agree = false;
            continue;
        }
        officer_record *ruler   = scenario->officers + record->ruler_officer_id;
        officer_record *advisor = scenario->officers + record->advisor_officer_id;
        if (ruler->force_id != record->id || ruler->status != OFFICER_STATUS_RULER) rulers_agree = false;
        if (advisor->force_id != record->id) advisors_agree = false;
    }
    TEST_CHECK(counts, rulers_agree, "the rulers of forces 0, 1, 9 and 10 are rulers of their force in the officer records");
    TEST_CHECK(counts, advisors_agree, "the advisors of forces 0, 1, 9 and 10 belong to their force");
}

/* scenario-file.md, City record. */
internal void
feature_scen002_city_tests(test_counts *counts, scenario_file *scenario)
{
    TEST_CHECK(counts, scenario->city_count == SCENARIO_CITY_COUNT, "SCEN002 has 42 cities");
    TEST_CHECK(counts, scenario->item_count == SAVE_ITEM_COUNT, "SCEN002 has 50 items");

    b32 owners_agree = true;
    for (u32 index = 0; index < ArrayCount(spec_scen002_city_owners); ++index)
    {
        spec_city_owner *expected = spec_scen002_city_owners + index;
        if (scenario->cities[expected->city_id].owner_force_id != expected->owner_force_id) owners_agree = false;
    }
    TEST_CHECK(counts, owners_agree, "the owner of each city is the force that scenario-file.md names");

    b32 commanders_none = true;
    for (u32 city = 0; city < scenario->city_count; ++city)
    {
        if (scenario->cities[city].commanding_officer_id != CITY_COMMANDER_NONE) commanders_none = false;
    }
    TEST_CHECK(counts, commanders_none, "a scenario city has no commander");

    /* The scenario field +n is the save field +n+3. save-file.md swapped the
       save fields +16 and +20 on 2026-10-17: +16 is the troops, +20 the gold.
       Thus +13 of the scenario is the troops and +17 the gold. */
    city_record *hanzhong = scenario->cities + SPEC_HANZHONG_CITY_ID;
    TEST_CHECK(counts, hanzhong->troops == 14000 && hanzhong->gold == 3200 && hanzhong->food == 48000,
               "Hanzhong starts with 14,000 troops, 3,200 gold and 48,000 food");
    city_record *chenliu = scenario->cities + SPEC_CHENLIU_CITY_ID;
    TEST_CHECK(counts, chenliu->troops == 28000 && chenliu->gold == 5600 && chenliu->food == 56000,
               "Chenliu starts with 28,000 troops and 5,600 gold, the gold of the city panel of the original");
    TEST_CHECK(counts, hanzhong->equipment_stocks[0] == 0 && hanzhong->equipment_stocks[1] == 6000 &&
                       hanzhong->equipment_stocks[2] == 2000 && hanzhong->equipment_stocks[3] == 0,
               "Hanzhong starts with equipment stocks 0, 6,000, 2,000, 0");
}

/* scenario-file.md, Officer record: SCEN002 against the save that started
   from it. The record is the same for bytes 0..147, and the static bytes
   agree for every officer. In sample.s11 the aptitudes, the liked and
   disliked lists, one portrait, the stats, the spouse and the sworn-brother
   head also differ from SCEN002 (QUESTIONS.md, Q-018). These tests assert
   only the fields that agree for all 850 officers. */
internal void
feature_scen002_against_sample_officer_tests(test_counts *counts, scenario_file *scenario, save_file *save)
{
    b32 names_agree = true, years_agree = true, family_agrees = true, compatibility_agrees = true;
    for (u32 slot = 0; slot < SPEC_SCENARIO_NAMED_OFFICERS; ++slot)
    {
        officer_record *start = scenario->officers + slot;
        officer_record *later = save->officers + slot;
        if (memcmp(start->family_name, later->family_name, OFFICER_FAMILY_NAME_SIZE) ||
            memcmp(start->given_name, later->given_name, OFFICER_GIVEN_NAME_SIZE) ||
            memcmp(start->courtesy_name, later->courtesy_name, OFFICER_COURTESY_NAME_SIZE)) names_agree = false;
        if (start->appearance_year != later->appearance_year || start->birth_year != later->birth_year ||
            start->death_year != later->death_year) years_agree = false;
        if (start->family_line_head != later->family_line_head || start->father != later->father ||
            start->related_officer != later->related_officer) family_agrees = false;
        if (start->compatibility != later->compatibility) compatibility_agrees = false;
    }
    TEST_CHECK(counts, names_agree, "officers 0..849 have the same names in SCEN002 and in sample.s11");
    TEST_CHECK(counts, years_agree, "officers 0..849 have the same appearance, birth and death years");
    TEST_CHECK(counts, family_agrees, "officers 0..849 have the same family head, father and related officer");
    TEST_CHECK(counts, compatibility_agrees, "officers 0..849 have the same compatibility");
}

/* scenario-file.md, City record: the scenario field at +n is the save field
   at +n+3. The capacity, the large-city flag and the durability maximum are
   constants of each city. */
internal void
feature_scen002_against_sample_city_tests(test_counts *counts, scenario_file *scenario, save_file *save)
{
    b32 capacity_agrees = true, large_flag_agrees = true, durability_agrees = true;
    for (u32 city = 0; city < SCENARIO_CITY_COUNT; ++city)
    {
        city_record *start = scenario->cities + city;
        city_record *later = save->cities + city;
        if (start->capacity_constant != later->capacity_constant) capacity_agrees = false;
        if (start->large_city_flag != later->large_city_flag) large_flag_agrees = false;
        if (start->durability_maximum != later->durability_maximum) durability_agrees = false;
    }
    TEST_CHECK(counts, capacity_agrees, "each city has the same capacity constant in SCEN002 and in sample.s11");
    TEST_CHECK(counts, large_flag_agrees, "each city has the same large-city flag in SCEN002 and in sample.s11");
    TEST_CHECK(counts, durability_agrees, "each city has the same durability maximum in SCEN002 and in sample.s11");
}

internal void
feature_scen002_against_sample_tests(test_counts *counts, memory_arena *arena, scenario_file *scenario)
{
    char path[MAX_PATH];
    test_fixture_path(path, MAX_PATH, SAMPLE_SAVE_FIXTURE);
    test_file sample = test_read_file(path, arena);
    save_file save = {};
    save_load_result loaded = {};
    if (sample.contents) loaded = save_load(arena, sample.contents, sample.size, &save);
    TEST_CHECK(counts, sample.contents && loaded.status == SAVE_LOAD_OK, "sample.s11 loads beside SCEN002");
    if (!sample.contents || loaded.status != SAVE_LOAD_OK) return;

    TEST_CHECK(counts, save.payload_scenario_number == scenario->header.scenario_number &&
                       save.payload_scenario_start_year == scenario->header.start_year &&
                       save.payload_scenario_start_month == scenario->header.start_month &&
                       save.payload_scenario_start_ten_day_period == scenario->header.start_ten_day_period,
               "the payload bytes 0..4 of sample.s11 are the start of SCEN002");
    feature_scen002_against_sample_officer_tests(counts, scenario, &save);
    feature_scen002_against_sample_city_tests(counts, scenario, &save);
}

internal void
feature_scenario_fixture_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(32));
    feature_name_index_fixture_tests(counts, &arena);

    char path[MAX_PATH];
    test_fixture_path(path, MAX_PATH, SCEN002_FIXTURE);
    test_file file = test_read_file(path, &arena);
    TEST_CHECK(counts, file.contents != 0, "SCEN002.s11 is in the fixtures");
    if (file.contents)
    {
        TEST_CHECK(counts, file.size == SCENARIO_FILE_SIZE, "SCEN002.s11 has 170,010 bytes");
        name_index not_a_name_index;
        TEST_CHECK(counts, name_index_load(file.contents, file.size, &not_a_name_index) == SCENARIO_LOAD_WRONG_KIND,
                   "SCEN002.s11 is not a name index");

        scenario_file scenario;
        scenario_load_status status = scenario_load(&arena, file.contents, file.size, &scenario);
        TEST_CHECK(counts, status == SCENARIO_LOAD_OK, "SCEN002.s11 loads as a scenario");
        if (status == SCENARIO_LOAD_OK)
        {
            feature_scen002_header_tests(counts, &scenario.header);
            feature_scen002_officer_tests(counts, &scenario);
            feature_scen002_force_tests(counts, &scenario);
            feature_scen002_city_tests(counts, &scenario);
            feature_scen002_against_sample_tests(counts, &arena, &scenario);
        }
    }
    test_arena_destroy(&arena);
}
