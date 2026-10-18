/* Unit tests of M6 combat (military-units-and-tactics.md, plots-and-states.md,
   military-facilities.md, skills.md): the parsers of tactics.tsv, plots.tsv,
   buildings.tsv and critical_skills.tsv, the assumed combat_damage rule and
   its hook, the class triangle and the cells of each tactic pattern. No
   engine, no files. */

#define UNIT_TACTIC_HEADER   "id\tkey\tname_zh\tunit_types\tgrade\tmorale\trange_min\trange_max\tpattern\teffect\tvalue\tdamage_percent\ttargets\talways_success\tstatus\n"
#define UNIT_PLOT_HEADER     "id\tkey\tname_zh\tmorale\trange\tarea\ttarget\teffect\tturns\tneeds_skill\tneeds_skill_2\tfrom_terrain\tnot_unit_types\tstatus\n"
#define UNIT_BUILDING_HEADER "id\tkey\tname_zh\tkind\tdurability\tgold\ttechnique\treplaces\trange_min\trange_max\tvalue\twater\tnatural\tstatus\n"
#define UNIT_CRITICAL_HEADER "id\tskill\tname_zh\taction\tunit_class\tcondition\tstatus\n"

/* Two unit types (pike 0, cavalry 1), one terrain (forest) and no techniques. */
internal void
unit_combat_content(sim_content *content)
{
    *content = {};
    content->unit_type_count = 2;
    memcpy(content->unit_types[0].key, "pike", sizeof("pike"));
    memcpy(content->unit_types[1].key, "cavalry", sizeof("cavalry"));
    content->terrain_count = 1;
    memcpy(content->terrains[0].key, "forest", sizeof("forest"));
}

internal void
unit_tactic_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content;
    unit_combat_content(&content);
    tsv_table table = {};
    char *text = content_test_text(arena, UNIT_TACTIC_HEADER
                                   "0\tpierce\tx\tpike\tany\t10\t1\t1\tsingle\tknock\t1\t110\tunits\tno\tdocumented\n"
                                   "1\tcharge\tx\tpike|cavalry\tA\t20\t1\t1\tfront3\tnone\t0\t130\tunits|buildings\tyes\tassumed\n");
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_tactics(&content, &table, &bad_row) && content.tactic_count == 2,
               "the tactic table parses");
    tactic_kind *charge = content.tactics + 1;
    TEST_CHECK(counts, content.tactics[0].unit_types == 1u && content.tactics[0].effect == TACTIC_KNOCK && content.tactics[0].aptitude == 0 &&
                       charge->unit_types == 3u && charge->aptitude == 2 && charge->pattern == PATTERN_FRONT3 &&
                       charge->targets == (TARGET_UNITS | TARGET_BUILDINGS) && charge->always_success && charge->damage_percent == 130,
               "a row has its unit types, its lowest aptitude, its pattern, its effect, its targets and its damage");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_TACTIC_HEADER "0\tx\tx\tarcher\tany\t10\t1\t1\tsingle\tnone\t0\t100\tunits\tno\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_tactics(&content, &bad, &bad_row), "a unit type that is not a unit type is a bad row");
    tsv_table bad_range = {};
    char *bad_range_text = content_test_text(arena, UNIT_TACTIC_HEADER "0\tx\tx\tpike\tany\t10\t3\t2\tsingle\tnone\t0\t100\tunits\tno\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_range_text, arena, &bad_range) && !content_load_tactics(&content, &bad_range, &bad_row),
               "a minimum range above the maximum is a bad row");
}

internal void
unit_plot_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content;
    unit_combat_content(&content);
    tsv_table table = {};
    char *text = content_test_text(arena, UNIT_PLOT_HEADER
                                   "0\tfire\tx\t10\t5\tsingle\thex\tfire\t3\t0\t0\t-\t-\tdocumented\n"
                                   "1\tambush\tx\t15\t5\tsingle\tenemy\tambush\t0\t0\t0\tforest\tcavalry\tdocumented\n"
                                   "2\tsorcery\tx\t20\t5\tsplash\tenemy\tsorcery\t2\t71\t72\t-\t-\tassumed\n");
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_plots(&content, &table, &bad_row) && content.plot_count == 3,
               "the plot table parses");
    TEST_CHECK(counts, content.plots[0].target == PLOT_TARGET_HEX && content.plots[1].from_terrain == 0 && content.plots[1].not_unit_types == 2u &&
                       content.plots[2].splash && content.plots[2].needs_skill == 71 && content.plots[2].needs_skill_2 == 72,
               "a row has its target, its terrain, the classes that cannot use it, its area and its skills");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_PLOT_HEADER "0\tx\tx\t10\t5\tsingle\tenemy\tambush\t0\t0\t0\tdesert\t-\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_plots(&content, &bad, &bad_row), "a terrain that is not a terrain is a bad row");
}

internal void
unit_building_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content;
    unit_combat_content(&content);
    tsv_table table = {};
    char *text = content_test_text(arena, UNIT_BUILDING_HEADER
                                   "0\tarrow_tower\tx\ttower\t1500\t300\t-\t-\t1\t2\t0\tno\tno\tdocumented\n"
                                   "1\trepeating_tower\tx\ttower\t2000\t500\t-\tarrow_tower\t1\t3\t0\tno\tno\tdocumented\n"
                                   "2\tdike\tx\tdike\t1000\t0\t-\t-\t1\t1\t0\tno\tyes\tassumed\n");
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_buildings(&content, &table, &bad_row) && content.building_count == 3,
               "the building table parses");
    TEST_CHECK(counts, content.buildings[0].type == BUILDING_TOWER && content.buildings[0].replaces == CONTENT_NO_BUILDING &&
                       content.buildings[1].replaces == 0 && content.buildings[1].range_max == 3 && content.buildings[2].natural,
               "a row has its kind, the building it replaces, its range and the natural flag");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_BUILDING_HEADER "0\tx\tx\ttower\t100\t0\t-\tnothing\t1\t1\t0\tno\tno\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_buildings(&content, &bad, &bad_row),
               "a replaced building that is not an earlier row is a bad row");
}

internal void
unit_critical_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content;
    unit_combat_content(&content);
    tsv_table table = {};
    char *text = content_test_text(arena, UNIT_CRITICAL_HEADER
                                   "0\t1\tx\tboth\tany\talways\tdocumented\n"
                                   "1\t53\tx\tattack\tcavalry\tmounted\tdocumented\n");
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_criticals(&content, &table, &bad_row) && content.critical_count == 2 &&
                       content.criticals[0].action == CRITICAL_BOTH && content.criticals[0].aptitude == CONTENT_ANY_APTITUDE &&
                       content.criticals[1].aptitude == OFFICER_APTITUDE_CAVALRY && content.criticals[1].condition == CRITICAL_MOUNTED,
               "the critical skill table parses: the skill, the action, the class and the condition");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_CRITICAL_HEADER "0\t101\tx\tboth\tany\talways\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_criticals(&content, &bad, &bad_row), "a skill number above 100 is a bad row");
}

/* A rule of a content pack: every hit takes 7 soldiers. */
internal u32
unit_flat_damage(sim_state *sim, sim_combat_input *input)
{
    (void)sim; (void)input;
    return 7;
}

internal void
unit_damage_rule_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = PushStruct(arena, sim_state);
    sim_init(sim, arena, 1);
    sim->content.rules.combat_damage_percent = 20;
    sim->content.rules.combat_min_damage     = 50;
    TEST_CHECK(counts, sim_combat_damage(sim, 1000, 60, 40, 100) == 120 && sim_combat_damage(sim, 1000, 60, 40, 150) == 180,
               "the assumed rule: soldiers x attack / (attack + defence) x 20 percent, times the percent of the attack");
    TEST_CHECK(counts, sim_combat_damage(sim, 100, 60, 40, 100) == 50 && sim_combat_damage(sim, 0, 60, 40, 100) == 0,
               "a hit takes at least 50 soldiers; a unit without soldiers does no damage");
    sim->ruleset.combat_damage = unit_flat_damage;
    TEST_CHECK(counts, sim_combat_damage(sim, 1000, 60, 40, 100) == 7, "a content pack can replace the rule through the hook");
}

internal void
unit_triangle_and_pattern_tests(test_counts *counts)
{
    TEST_CHECK(counts, sim_class_beats(OFFICER_APTITUDE_SPEAR, OFFICER_APTITUDE_CAVALRY) && sim_class_beats(OFFICER_APTITUDE_CAVALRY, OFFICER_APTITUDE_HALBERD) &&
                       sim_class_beats(OFFICER_APTITUDE_HALBERD, OFFICER_APTITUDE_SPEAR) && !sim_class_beats(OFFICER_APTITUDE_CAVALRY, OFFICER_APTITUDE_SPEAR) &&
                       !sim_class_beats(OFFICER_APTITUDE_CROSSBOW, OFFICER_APTITUDE_SPEAR),
               "the triangle: pike beats cavalry, cavalry beats halberd, halberd beats pike");
    sim_unit unit = {};
    unit.hex = hex_make(5, 5);
    tactic_kind tactic = {};
    hex_coord cells[16];
    hex_coord next = hex_neighbor(unit.hex, 0);
    tactic.pattern = PATTERN_SINGLE;
    u32 single = sim_tactic_cells(&unit, &tactic, next, cells);
    tactic.pattern = PATTERN_FRONT3;
    u32 front = sim_tactic_cells(&unit, &tactic, next, cells);
    b32 front_ok = front == 3;
    for (u32 index = 0; index < front; ++index) front_ok = front_ok && hex_distance(cells[index], unit.hex) == 1;
    tactic.pattern = PATTERN_AROUND;
    u32 around = sim_tactic_cells(&unit, &tactic, next, cells);
    tactic.pattern = PATTERN_SPLASH;
    u32 splash = sim_tactic_cells(&unit, &tactic, next, cells);
    tactic.pattern = PATTERN_LINE;
    u32 line = sim_tactic_cells(&unit, &tactic, hex_neighbor(next, 0), cells);
    TEST_CHECK(counts, single == 1 && front_ok && around == 6 && splash == 7 && line == 2 && hex_equal(cells[1], hex_neighbor(next, 0)),
               "the patterns: one hex, the front three, the six around, a splash of seven, and a line to the target");
}

/* The turns until unit 0 recovers from 混亂, with the PRNG seeded. */
internal u32
unit_confusion_turns(sim_state *sim, u64 seed)
{
    sim_event buffer[16];
    sim_event_sink sink = { buffer, 0, ArrayCount(buffer) };
    sim->rng = seed;
    sim->units[0].state = UNIT_STATE_CONFUSED;
    sim->units[0].state_turns = 0;
    for (u32 turn = 1; turn <= 50; ++turn)
    {
        sink.count = 0;
        sim_run_states(sim, 0, &sink);
        if (sim->units[0].state == UNIT_STATE_NONE) return turn;
    }
    return 50;
}

/* Q-218 (knowledge/combat.md, "Confusion"; the rules are assumed): a unit
   in 混亂 recovers by a chance each turn, the highest 智力 of the unit
   between the floor and the cap. A small unit next to a large enemy can
   panic. */
internal void
unit_confusion_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_march_world(arena);
    sim_rules *rules = &sim->content.rules;
    rules->confusion_recover_floor = 20;
    rules->confusion_recover_cap   = 90;
    sim->unit_count = 1;
    sim_unit *unit = sim->units;
    *unit = {};
    hex_offset at = { 5, 5 };
    unit->alive = true; unit->force_id = 0; unit->hex = hex_from_offset(at); unit->unit_type = 0; unit->soldiers = 3000;
    unit->officers[0] = 0; unit->officers[1] = SIM_NO_OFFICER; unit->officers[2] = SIM_NO_OFFICER;
    officer_component *commander = sim_find_officer(sim, 0);

    commander->intelligence = 90;
    u32 quick = 0, high_total = 0;
    for (u64 seed = 1; seed <= 100; ++seed)
    {
        u32 turns = unit_confusion_turns(sim, seed * 7919);
        quick += turns <= 2;
        high_total += turns;
    }
    commander->intelligence = 30;
    u32 slow = 0, low_total = 0;
    for (u64 seed = 1; seed <= 100; ++seed)
    {
        u32 turns = unit_confusion_turns(sim, seed * 7919);
        slow += turns >= 3;
        low_total += turns;
    }
    TEST_CHECK(counts, quick >= 90, "a unit with 智力 90 recovers from 混亂 within 2 turns in most seeds (90 percent each turn)");
    TEST_CHECK(counts, low_total > 2 * high_total && slow >= 30, "a unit with 智力 30 takes longer: often 3 turns or more");
    commander->intelligence = 5;
    u32 floor_chance = sim_confusion_recover_chance(sim, unit);
    commander->intelligence = 99;
    TEST_CHECK(counts, floor_chance == 20 && sim_confusion_recover_chance(sim, unit) == 90, "the chance stays between confusion.recover_floor and confusion.recover_cap");

    /* 偽報 still counts down its turns. */
    sim->rng = 1;
    unit->state = UNIT_STATE_FALSE_REPORT;
    unit->state_turns = 2;
    sim_event buffer[16];
    sim_event_sink sink = { buffer, 0, ArrayCount(buffer) };
    sim_run_states(sim, 0, &sink);
    b32 still = unit->state == UNIT_STATE_FALSE_REPORT;
    sink.count = 0;
    sim_run_states(sim, 0, &sink);
    TEST_CHECK(counts, still && unit->state == UNIT_STATE_NONE, "偽報 ends after its turns");

    /* The panic: 400 soldiers next to an enemy of 5000 (5 times or more). */
    rules->panic_soldiers = 500;
    rules->panic_ratio    = 5;
    rules->panic_chance   = 100;
    unit->soldiers = 400;
    sim->unit_count = 2;
    sim->units[1] = {};
    sim->units[1].alive = true; sim->units[1].force_id = 1; sim->units[1].hex = hex_neighbor(unit->hex, 0);
    sim->units[1].unit_type = 0; sim->units[1].soldiers = 5000;
    sim->units[1].officers[0] = 1; sim->units[1].officers[1] = SIM_NO_OFFICER; sim->units[1].officers[2] = SIM_NO_OFFICER;
    sink.count = 0;
    sim_run_states(sim, 0, &sink);
    b32 panicked = unit->state == UNIT_STATE_CONFUSED;
    unit->state = UNIT_STATE_NONE;
    sim->units[1].soldiers = 1500;   /* less than 5 times */
    sink.count = 0;
    sim_run_states(sim, 0, &sink);
    b32 calm_small_enemy = unit->state == UNIT_STATE_NONE;
    sim->units[1].soldiers = 5000;
    unit->soldiers = 600;            /* more than panic.soldiers */
    sink.count = 0;
    sim_run_states(sim, 0, &sink);
    b32 calm_large_unit = unit->state == UNIT_STATE_NONE;
    TEST_CHECK(counts, panicked && calm_small_enemy && calm_large_unit,
               "a unit of 500 soldiers or fewer next to an enemy of 5 times its soldiers can confuse itself; a smaller enemy or a larger unit does not");
    rules->panic_chance = 0;
    unit->soldiers = 400;
    sink.count = 0;
    sim_run_states(sim, 0, &sink);
    TEST_CHECK(counts, unit->state == UNIT_STATE_NONE, "panic.chance 0 turns the panic off");
}

internal void
unit_combat_tests(test_counts *counts, memory_arena *arena)
{
    unit_confusion_tests(counts, arena);
    unit_tactic_table_tests(counts, arena);
    unit_plot_table_tests(counts, arena);
    unit_building_table_tests(counts, arena);
    unit_critical_table_tests(counts, arena);
    unit_damage_rule_tests(counts, arena);
    unit_triangle_and_pattern_tests(counts);
}
