/* Unit tests for code/sim/scenario_format: a hand-built SCEN0nn.s11 and a
   hand-built Scenario.s11. Offsets are plain numbers from the tables of
   spec-src/spec/formats/scenario-file.md, as in save_format_tests.cpp.
   The real files are not fixtures yet (QUESTIONS.md, Q-014). */

internal void
test_build_scenario_container(u8 *file, u32 file_kind)
{
    file[0x002] = 0xFE; file[0x003] = 0xFF;          /* 00 00 FE FF */
    test_put_u32(file, 0x004, file_kind);            /* file kind */
    memcpy(file + 0x008, "KOEI%SAN11", 10);
    test_put_u32(file, 0x018, 1);
    test_put_u32(file, 0x01C, 2);
}

/* SCEN002 values from scenario-file.md where the spec gives them. */
internal void
test_build_scenario_file(u8 *file)
{
    memset(file, 0, 170010);
    test_build_scenario_container(file, 0x16);
    file[0x05A] = 2;                                 /* scenario number */
    test_put_u16(file, 0x05B, 194);                  /* start year */
    file[0x05D] = 6;                                 /* start month */
    file[0x05E] = 1;                                 /* start ten-day period */
    u8 title[] = { 0xB8, 0x73, 0xB6, 0xAF, 0xB3, 0xCE, 0xBE, 0xDA, 0x00, 0xA4, 0x40 };   /* title, NUL, next text */
    memcpy(file + 0x05F, title, sizeof(title));

    /* Officer 343 at 0x04560 + 152 * 343: the save record cut at +152. */
    u8 save_record[203];
    test_build_officer_record(save_record);
    memcpy(file + 0x04560 + 152 * 343, save_record, 152);

    test_put_u16(file, 0x23E10 + 57 * 1 + 53, 343);   /* item 1: owner 343 */

    u8 *force = file + 0x25454 + 72 * 9;              /* force 9 */
    test_put_u16(force, 0, 442);                      /* +0 ruler officer id */
    test_put_u16(force, 2, 450);                      /* +2 advisor officer id */
    for (u32 other = 0; other < 46; ++other) force[4 + other] = 50;   /* +4 relations */
    force[4 + 9] = 100;

    u8 *city = file + 0x262FC + 81 * 36;              /* city 36 */
    memset(city, 0xEE, 8);                            /* +0 unknown */
    city[8] = 9;                                      /* +8 owner force */
    test_put_u32(city, 9, 100000);                    /* +9 capacity constant */
    test_put_u32(city, 13, 14000);                    /* +13 starting troops (save +16, save-file.md) */
    test_put_u32(city, 17, 3200);                     /* +17 starting gold (save +20) */
    test_put_u32(city, 21, 48000);                    /* +21 starting food */
    u32 stocks[10] = { 0, 6000, 2000, 0, 0, 0, 0, 0, 0, 0 };
    for (u32 stock = 0; stock < 10; ++stock) test_put_u32(city, 29 + 4 * stock, stocks[stock]);   /* +29 equipment */
    city[73] = 50;                                    /* +73 city constant */
    city[74] = 1;                                     /* +74 large-city flag */
    test_put_u16(city, 77, 8000);                     /* +77 durability maximum */
}

internal void
unit_scenario_file_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(2));
    u8 *file = (u8 *)arena_push_size(&arena, 170010, 1);
    test_build_scenario_file(file);

    scenario_file scenario;
    scenario_load_status status = scenario_load(&arena, file, 170010, &scenario);
    TEST_CHECK(counts, status == SCENARIO_LOAD_OK, "a hand-built scenario loads");
    TEST_CHECK(counts, scenario.header.file_kind == 0x16 && scenario.header.constant_marker[2] == 0xFE &&
                       scenario.header.constant_at_0x018 == 1 && scenario.header.constant_at_0x01c == 2,
               "scenario container constants and file kind");
    TEST_CHECK(counts, scenario.header.scenario_number == 2 && scenario.header.start_year == 194 &&
                       scenario.header.start_month == 6 && scenario.header.start_ten_day_period == 1,
               "scenario number and start date at 0x05A..0x05E");
    TEST_CHECK(counts, scenario.header.scenario_title[0] == 0xB8 && scenario.header.scenario_title[7] == 0xDA &&
                       scenario.header.scenario_title[8] == 0 && scenario.header.scenario_title[9] == 0,
               "the title stops at its NUL and the next text does not leak in");

    TEST_CHECK(counts, scenario.officer_slot_count == 850 && scenario.item_count == 50 &&
                       scenario.force_count == 47 && scenario.city_count == 42, "table sizes of the layout");
    officer_record *officer = scenario.officers + 343;
    TEST_CHECK(counts, officer->id == 343 && officer->birth_year == 155 && officer->death_year == 220 &&
                       officer->leadership == 96 && officer->charm == 96 && officer->force_id == 10 &&
                       officer->status == OFFICER_STATUS_REGULAR_OFFICER, "scenario officer 343 decodes like the save record");
    TEST_CHECK(counts, officer->has_acted_this_turn && !officer->has_save_fields && officer->order_kind == 0,
               "scenario officer keeps +148 and has no save-only fields");
    TEST_CHECK(counts, officer->has_loyalty && officer->loyalty == 97, "scenario officer +104 loyalty, a byte of the shared part");
    TEST_CHECK(counts, officer_slot_is_empty(scenario.officers + 0), "a zero slot is empty");
    TEST_CHECK(counts, scenario.items[1].owner_officer_id == 343, "scenario items use the save item layout");

    force_record *force = scenario.forces + 9;
    TEST_CHECK(counts, force->id == 9 && force->ruler_officer_id == 442 && force->advisor_officer_id == 450,
               "force +0 ruler and +2 advisor");
    TEST_CHECK(counts, force->relations[0] == 50 && force->relations[9] == 100 && force->relations[45] == 50,
               "force +4 relations, own entry 100");

    city_record *city = scenario.cities + 36;
    TEST_CHECK(counts, city->id == 36 && city->owner_force_id == 9, "scenario city +8 owner force");
    TEST_CHECK(counts, city->capacity_constant == 100000 && city->troops == 14000 && city->gold == 3200 && city->food == 48000,
               "scenario city +9 capacity, +13 troops, +17 gold, +21 food");
    TEST_CHECK(counts, city->equipment_stocks[CITY_STOCK_HALBERDS] == 6000 && city->equipment_stocks[CITY_STOCK_CROSSBOWS] == 2000,
               "scenario city +29 equipment stocks");
    TEST_CHECK(counts, city->large_city_flag == 1 && city->durability_maximum == 8000, "scenario city +74 and +77");
    TEST_CHECK(counts, city->commanding_officer_id == CITY_COMMANDER_NONE && city->flags == 0 && city->public_order == 0,
               "a scenario city has no commander, flags or public order");

    TEST_CHECK(counts, scenario_load(&arena, file, 0x27046 - 1, &scenario) == SCENARIO_LOAD_FILE_TOO_SMALL,
               "a file that stops inside the city table is refused");
    TEST_CHECK(counts, scenario_load(&arena, file, 0x27046, &scenario) == SCENARIO_LOAD_OK, "a file that ends after the city table loads");
    test_put_u32(file, 0x004, 0x1A);
    TEST_CHECK(counts, scenario_load(&arena, file, 170010, &scenario) == SCENARIO_LOAD_WRONG_KIND, "a save is not a scenario");
    test_put_u32(file, 0x004, 0x16);
    file[0x008] = 'X';
    TEST_CHECK(counts, scenario_load(&arena, file, 170010, &scenario) == SCENARIO_LOAD_WRONG_KIND, "a wrong magic text is refused");
    TEST_CHECK(counts, strcmp(scenario_load_status_name(SCENARIO_LOAD_WRONG_KIND), "wrong kind") == 0, "scenario status name");
    test_arena_destroy(&arena);
}

internal void
unit_name_index_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(1));
    u8 *file = (u8 *)arena_push_size(&arena, 47928, 1);
    test_build_scenario_container(file, 0x18);

    u8 *city = file + 0x05A + 25 * 36;                /* city 36 */
    city[0] = 0xBA; city[1] = 0x7E; city[2] = 0xA4; city[3] = 0xA4; city[4] = 0x00;   /* +0 name, NUL terminated */
    city[5] = 0x82; city[6] = 0xA0;                   /* leftovers before +17 */
    city[17] = 5;                                     /* +17 province index */
    u8 adjacent[7] = { 17, 18, 20, 37, 0xFF, 0x33, 0x33 };   /* +18 adjacent ids, 0xFF terminated */
    memcpy(city + 18, adjacent, 7);
    u8 *full = file + 0x05A + 25 * 1;                 /* city 1: seven neighbours, no terminator */
    for (u32 slot = 0; slot < 7; ++slot) full[18 + slot] = (u8)slot;

    u8 *gate = file + 0x474 + 26 * 1;                 /* entry 1 = location 43 */
    gate[0] = 0xAA; gate[1] = 0xEA; gate[2] = 0x00; gate[3] = 0x77;

    name_index index;
    scenario_load_status status = name_index_load(file, 47928, &index);
    TEST_CHECK(counts, status == SCENARIO_LOAD_OK && index.file_kind == 0x18, "a hand-built name index loads");
    name_index_city *entry = index.cities + 36;
    TEST_CHECK(counts, entry->id == 36 && entry->name[0] == 0xBA && entry->name[3] == 0xA4, "city entry name bytes");
    TEST_CHECK(counts, entry->name[5] == 0 && entry->name[6] == 0, "bytes after the NUL are not kept");
    TEST_CHECK(counts, entry->province_index == 5, "city entry +17 province index");
    TEST_CHECK(counts, entry->adjacent_count == 4 && entry->adjacent_city_ids[0] == 17 && entry->adjacent_city_ids[3] == 37,
               "city entry +18 adjacent ids stop at 0xFF");
    TEST_CHECK(counts, index.cities[1].adjacent_count == 7 && index.cities[1].adjacent_city_ids[6] == 6,
               "seven neighbours fill the list without a terminator");
    TEST_CHECK(counts, index.gates_and_ports[0].id == 42 && index.gates_and_ports[44].id == 86, "gates and ports are locations 42..86");
    TEST_CHECK(counts, index.gates_and_ports[1].name[0] == 0xAA && index.gates_and_ports[1].name[2] == 0 &&
                       index.gates_and_ports[1].name[3] == 0, "gate entry name stops at its NUL");

    TEST_CHECK(counts, name_index_load(file, 0x906 - 1, &index) == SCENARIO_LOAD_FILE_TOO_SMALL,
               "a name index that stops inside the gate table is refused");
    test_put_u32(file, 0x004, 0x16);
    TEST_CHECK(counts, name_index_load(file, 47928, &index) == SCENARIO_LOAD_WRONG_KIND, "a scenario is not a name index");
    test_arena_destroy(&arena);
}

internal void
unit_scenario_format_tests(test_counts *counts)
{
    unit_scenario_file_tests(counts);
    unit_name_index_tests(counts);
}
