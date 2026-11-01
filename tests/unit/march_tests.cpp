/* Unit tests of M5 march (terrain-and-map.md, military-units-and-tactics.md):
   the parsers of terrain.tsv and unit_types.tsv, the assumed formula of the
   unit values, and the move range with the zone of control on a small world
   that the test builds. No engine, no files. */

#define UNIT_TERRAIN_HEADER "id\tkey\tcost_foot\tcost_cavalry\tcost_siege\tcost_navy\tneeds_technique\tloss_percent\tloss_waiver_technique\t" \
                            "loss_waiver_skill\tbuildable\ttactics\ttile\tlevel\n"
#define UNIT_TYPE_HEADER    "id\tkey\tweapon\taptitude\tmovement_class\tmove\tattack\tdefence\tdestroy\trange\tcan_attack\tcounterattack\ttransport\n"

internal void
unit_terrain_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    content.technique_count = 1;
    memcpy(content.techniques[0].key, "rough_march", sizeof("rough_march"));
    tsv_table table = {};
    char *text = content_test_text(arena, UNIT_TERRAIN_HEADER
                                   "0\tgrass\t4\t4\t4\t0\t-\t0\t-\t0\tyes\tpike|halberd|crossbow|cavalry|siege\t1\t1\n"
                                   "1\tplank_road\t8\t8\t10\t0\t-\t10\trough_march\t7\tno\tpike|halberd\t5\t3\n"
                                   "2\tcliff\t0\t0\t0\t0\t-\t0\t-\t0\tno\t-\t4\t3\n");
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_terrain(&content, &table, &bad_row) && content.terrain_count == 3,
               "the terrain table parses");
    terrain_kind *plank = content.terrains + 1;
    TEST_CHECK(counts, plank->cost[MOVEMENT_FOOT] == 8 && plank->cost[MOVEMENT_SIEGE] == 10 && plank->loss_percent == 10 &&
                       plank->loss_waiver_technique == 0 && plank->loss_waiver_skill == 7 && !plank->buildable &&
                       plank->tactics == ((1u << TACTIC_PIKE) | (1u << TACTIC_HALBERD)),
               "a row has the cost of each class, the loss, its waivers and the classes of its tactics");
    TEST_CHECK(counts, content.terrains[2].tactics == 0 && content.terrains[2].cost[MOVEMENT_FOOT] == 0 && content_find_terrain(&content, "cliff") == 2,
               "a cliff takes no unit and no tactic");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_TERRAIN_HEADER "0\tx\t4\t4\t4\t0\t-\t0\t-\t0\tyes\tpike|archer\t1\t1\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_terrain(&content, &bad, &bad_row), "a tactic class that is not a class is a bad row");
    tsv_table bad_tech = {};
    char *bad_tech_text = content_test_text(arena, UNIT_TERRAIN_HEADER "0\tx\t4\t4\t4\t0\tmagic\t0\t-\t0\tyes\t-\t1\t1\n");
    TEST_CHECK(counts, tsv_parse(bad_tech_text, arena, &bad_tech) && !content_load_terrain(&content, &bad_tech, &bad_row),
               "a technique key that is not a technique is a bad row");
}

internal void
unit_type_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    content.weapon_count = 1;
    memcpy(content.weapons[0].key, "pike", sizeof("pike"));
    tsv_table table = {};
    char *text = content_test_text(arena, UNIT_TYPE_HEADER
                                   "0\tsword\t-\tnone\tfoot\t22\tE\tE\t-\t1\tyes\tyes\tno\n"
                                   "1\tpike\tpike\tspear\tfoot\t22\tA\tA\t-\t1\tyes\tyes\tno\n");
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_unit_types(&content, &table, &bad_row) && content.unit_type_count == 2,
               "the unit type table parses");
    TEST_CHECK(counts, content.unit_types[0].weapon == CONTENT_NO_WEAPON && content.unit_types[0].aptitude == CONTENT_APTITUDE_NONE &&
                       content.unit_types[1].weapon == 0 && content.unit_types[1].aptitude == OFFICER_APTITUDE_SPEAR &&
                       content.unit_types[1].attack == GRADE_A && content.unit_types[0].destroy == GRADE_NONE,
               "a row names its weapon, its aptitude and its grades");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_TYPE_HEADER "0\tbow\tbow\tcrossbow\tfoot\t22\tC\tC\t-\t2\tyes\tyes\tno\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_unit_types(&content, &bad, &bad_row), "a weapon that is not a weapon is a bad row");
    tsv_table bad_grade = {};
    char *bad_grade_text = content_test_text(arena, UNIT_TYPE_HEADER "0\tx\t-\tnone\tfoot\t22\tZ\tE\t-\t1\tyes\tyes\tno\n");
    TEST_CHECK(counts, tsv_parse(bad_grade_text, arena, &bad_grade) && !content_load_unit_types(&content, &bad_grade, &bad_row),
               "a grade that is not S to E or - is a bad row");
}

/* A small world: a 12 x 12 map of one terrain (cost 4 for foot), a foot
   class with movement 22, three officers, and the rules of the values. */
internal sim_state *
unit_march_world(memory_arena *arena)
{
    sim_state *sim = PushStruct(arena, sim_state);
    sim_init(sim, arena, 1);
    sim_init_map(sim, arena, 12, 12, 1);
    sim->content.terrain_count = 1;
    sim->content.terrains[0].cost[MOVEMENT_FOOT] = 4;
    sim->content.terrains[0].needs_technique       = CONTENT_NO_TECHNIQUE;
    sim->content.terrains[0].loss_waiver_technique = CONTENT_NO_TECHNIQUE;
    sim->content.unit_type_count = 1;
    unit_type_kind *pike = sim->content.unit_types;
    pike->weapon = CONTENT_NO_WEAPON; pike->aptitude = OFFICER_APTITUDE_SPEAR; pike->movement = MOVEMENT_FOOT;
    pike->move = 22; pike->attack = GRADE_A; pike->defence = GRADE_A; pike->range = 1;
    sim_rules *rules = &sim->content.rules;
    rules->unit_deputy_percent = 50;
    rules->grade_percent[GRADE_A] = 110;
    rules->aptitude_percent[0] = 90; rules->aptitude_percent[1] = 100; rules->aptitude_percent[2] = 110; rules->aptitude_percent[3] = 120;
    for (u16 id = 0; id < 3; ++id)
    {
        officer_component officer = {};
        officer.officer_id = id;
        officer.force_id   = 0;
        officer.status     = 3;
        officer.job_city   = SIM_NO_CITY;
        officer.spouse     = 0xFFFF;
        officer.sworn_brother_group_head = 0xFFFF;
        sim_add_officer(sim, officer);
    }
    return sim;
}

internal void
unit_values_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_march_world(arena);
    officer_component *commander = sim_find_officer(sim, 0), *deputy = sim_find_officer(sim, 1);
    commander->war = 60; commander->leadership = 70; commander->intelligence = 50; commander->politics = 40;
    deputy->war = 80; deputy->leadership = 50; deputy->intelligence = 90; deputy->politics = 60;
    commander->unit_aptitudes[OFFICER_APTITUDE_SPEAR] = 1;   /* B */
    deputy->unit_aptitudes[OFFICER_APTITUDE_SPEAR]    = 2;   /* A */
    u16 officers[3] = { 0, 1, SIM_NO_OFFICER };
    sim_unit_values values = sim_unit_values_of(sim, 0, 0, officers);
    /* force 60 + half of (80 - 60) = 70; x aptitude A 110% x grade A 110% = 84 */
    TEST_CHECK(counts, values.aptitude == 2 && values.attack == 70 * 110 * 110 / 10000,
               "attack: the commander's force plus half the excess of a better deputy, times the best aptitude and the class grade (assumed)");
    TEST_CHECK(counts, values.defence == 70 * 110 * 110 / 10000, "defence: a lower deputy adds nothing to the leadership of the commander");
    TEST_CHECK(counts, values.intellect == 90 && values.build == 50 && values.move == 22,
               "intellect: the best of the officers; build: politics by the deputy rule; move: the class movement");
    commander->spouse = 1;
    values = sim_unit_values_of(sim, 0, 0, officers);
    TEST_CHECK(counts, values.attack == 80 * 110 * 110 / 10000, "a couple gives the full force of the better officer");
    u16 alone[3] = { 2, SIM_NO_OFFICER, SIM_NO_OFFICER };
    TEST_CHECK(counts, sim_unit_values_of(sim, 0, 0, alone).aptitude == 0 && sim_unit_values_of(sim, 0, 7, alone).move == 0,
               "an officer without aptitude has C; an unknown class has no values");
}

internal void
unit_reach_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_march_world(arena);
    hex_offset start_offset = { 2, 6 };
    hex_coord start = hex_from_offset(start_offset);
    sim->unit_count = 1;
    sim_unit *unit = sim->units;
    *unit = {};
    unit->alive = true; unit->force_id = 0; unit->hex = start; unit->unit_type = 0;
    unit->officers[0] = 0; unit->officers[1] = SIM_NO_OFFICER; unit->officers[2] = SIM_NO_OFFICER;

    sim_reach reach = sim_unit_reach(sim, 0, &sim->scratch);
    b32 within = reach.count > 1 && reach.costs[0] == 0 && hex_equal(reach.cells[0], start);
    u32 farthest = 0;
    for (u32 index = 0; index < reach.count; ++index)
    {
        if (reach.costs[index] > 22 || reach.costs[index] != 4 * (u32)hex_distance(start, reach.cells[index])) within = false;
        if (hex_distance(start, reach.cells[index]) > (s32)farthest) farthest = (u32)hex_distance(start, reach.cells[index]);
    }
    TEST_CHECK(counts, within && farthest == 5, "on open ground at cost 4 a unit with movement 22 reaches 5 hexes; each cost is 4 x the distance");

    /* An enemy two hexes east: the hex between is in its zone and ends the move. */
    hex_coord east1 = hex_neighbor(start, 0), east2 = hex_neighbor(east1, 0), east3 = hex_neighbor(east2, 0);
    hex_coord enemy_hex = hex_neighbor(east2, 5);
    sim->unit_count = 2;
    sim->units[1] = {};
    sim->units[1].alive = true; sim->units[1].force_id = 1; sim->units[1].hex = enemy_hex; sim->units[1].unit_type = CONTENT_NO_UNIT_TYPE;
    arena_reset(&sim->scratch);
    reach = sim_unit_reach(sim, 0, &sim->scratch);
    u32 place = sim_reach_find(&reach, east2);
    TEST_CHECK(counts, place != U32_MAX && reach.from[place] != U32_MAX && hex_equal(reach.cells[reach.from[place]], east1),
               "the unit can enter a hex of the zone of control");
    u32 beyond = sim_reach_find(&reach, east3);
    TEST_CHECK(counts, beyond == U32_MAX || !hex_equal(reach.cells[reach.from[beyond]], east2),
               "the unit does not go on from a hex of the zone of control");
    TEST_CHECK(counts, sim_reach_find(&reach, enemy_hex) == U32_MAX, "the hex of another unit is not in the range");

    sim->officers.data[0].skill_number = 0;   /* 飛將, skills.md #1 */
    sim->content.skills[0].traits = content_trait_bit(TRAIT_ZOC_LAND);
    arena_reset(&sim->scratch);
    reach = sim_unit_reach(sim, 0, &sim->scratch);
    beyond = sim_reach_find(&reach, east3);
    TEST_CHECK(counts, beyond != U32_MAX && reach.costs[beyond] == 12, "with 飛將 the unit passes the zone of control on land");
}

internal void
unit_march_tests(test_counts *counts, memory_arena *arena)
{
    unit_terrain_table_tests(counts, arena);
    unit_type_table_tests(counts, arena);
    arena_reset(arena);
    unit_values_tests(counts, arena);
    arena_reset(arena);
    unit_reach_tests(counts, arena);
}
