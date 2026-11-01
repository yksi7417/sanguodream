/* Unit tests of the skill traits (docs/SKILLS.md): the parser of the column
   traits and of plot_skills.tsv, a new skill of a pack, and the behaviour of
   each group of traits on the units of a small world. No engine, no files. */

#define UNIT_TRAIT_SKILL_HEADER "number\tkey\tname_zh\tcategory\ttraits\tnote\tfile_id\n"
#define UNIT_PLOT_SKILL_HEADER  "id\tskill\tname_zh\tplot\tcondition\toutcome\tstatus\n"

/* The skill number holds the traits, and the officer holds the skill. */
internal void
unit_give_traits(sim_state *sim, u16 officer_id, u32 number, u64 traits)
{
    sim->content.skills[number - 1].traits = traits;
    sim_find_officer(sim, officer_id)->skill_number = (u8)(number - 1);
}

internal void
unit_clear_skill(sim_state *sim, u16 officer_id)
{
    sim_find_officer(sim, officer_id)->skill_number = OFFICER_SKILL_NONE;
}

/* Unit 0 of force 0 and unit 1 of force 1, next to each other. */
internal sim_state *
unit_trait_world(memory_arena *arena)
{
    sim_state *sim = unit_march_world(arena);
    sim_rules *rules = &sim->content.rules;
    rules->fire_damage_percent      = 10;
    rules->fire_turns               = 3;
    rules->skill_fire_power_percent = 200;
    rules->skill_chance_percent     = 100;   /* the tests do not roll */
    rules->skill_morale_gain        = 10;
    rules->skill_few_soldiers       = 3000;
    rules->skill_weak_attack        = 500;
    rules->skill_support_percent    = 30;
    rules->skill_heart_attack_percent = 15;
    rules->plot_base_success        = 50;
    rules->plot_critical_morale     = 10;
    rules->combat_damage_percent    = 20;
    rules->combat_min_damage        = 50;
    rules->combat_critical_percent  = 150;
    rules->combat_critical_chance   = 0;
    rules->morale_max               = 100;
    rules->unit_soldier_cap         = 15000;
    sim->content.terrains[0].burnable = true;
    sim->unit_count = 2;
    hex_offset at = { 5, 5 };
    for (u32 index = 0; index < 2; ++index)
    {
        sim_unit *unit = sim->units + index;
        *unit = {};
        unit->alive = true; unit->force_id = (u8)index; unit->unit_type = 0; unit->soldiers = 5000; unit->morale = 50;
        unit->hex = index ? hex_neighbor(hex_from_offset(at), 0) : hex_from_offset(at);
        unit->officers[0] = (u16)index; unit->officers[1] = SIM_NO_OFFICER; unit->officers[2] = SIM_NO_OFFICER;
        unit_clear_skill(sim, (u16)index);
    }
    return sim;
}

internal void
unit_trait_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    u32 bad_row = 0;
    /* A pack adds a skill of its own: its row has the behaviours of two old skills. */
    char *text = content_test_text(arena, UNIT_TRAIT_SKILL_HEADER
                                   "1\tfire_ace\tx\tplot\tfire_immune|fire_power|fire_sure\tx\t0\n"
                                   "2\tplain\tx\tattack\t-\tx\t1\n"
                                   "3\tpack_only\tx\tattack\tdouble_attack|rattan\tx\t150\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_skills(&content, &table, &bad_row) && content.skill_count == 3,
               "a pack can have any count of skills, up to the capacity");
    TEST_CHECK(counts, content.skills[0].traits == (content_trait_bit(TRAIT_FIRE_IMMUNE) | content_trait_bit(TRAIT_FIRE_POWER) | content_trait_bit(TRAIT_FIRE_SURE)) &&
                       content.skills[1].traits == 0 && !content.skills[1].hooked && content.skills[2].hooked,
               "the column traits has one or more traits with | between them; - is no trait");
    TEST_CHECK(counts, content_skill_of_file_id(&content, 150) == 2, "a new skill has its own file id above 99");

    sim_content plots = {};
    plots.plot_count = 2;
    memcpy(plots.plots[0].key, "fire", sizeof("fire"));
    memcpy(plots.plots[1].key, "calm", sizeof("calm"));
    tsv_table rows = {};
    char *row_text = content_test_text(arena, UNIT_PLOT_SKILL_HEADER "0\t5\tx\tfire\tlower_intellect\tcritical\tx\n1\t6\tx\tany\talways\tsure\tx\n");
    TEST_CHECK(counts, tsv_parse(row_text, arena, &rows) && content_load_plot_skills(&plots, &rows, &bad_row) && plots.plot_skill_count == 2 &&
                       plots.plot_skills[0].plot == 0 && plots.plot_skills[0].condition == PLOT_SKILL_LOWER_INTELLECT && plots.plot_skills[0].outcome == PLOT_SKILL_CRITICAL &&
                       plots.plot_skills[1].plot == CONTENT_ANY_PLOT && plots.plot_skills[1].outcome == PLOT_SKILL_SURE,
               "plot_skills.tsv: the skill, the plot, the condition and the outcome");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_PLOT_SKILL_HEADER "0\t5\tx\tno_such_plot\talways\tsure\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_plot_skills(&plots, &bad, &bad_row), "a plot that is not in plots.tsv is a bad row");
}

/* 火神: any officer with a skill that has the three traits. The holder can be a deputy. */
internal void
unit_fire_trait_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_trait_world(arena);
    sim_event buffer[64];
    sim_event_sink sink = { buffer, 0, ArrayCount(buffer) };
    u64 fire_god = content_trait_bit(TRAIT_FIRE_IMMUNE) | content_trait_bit(TRAIT_FIRE_POWER) | content_trait_bit(TRAIT_FIRE_SURE);
    sim_unit *mine = sim->units, *enemy = sim->units + 1;
    u32 enemy_cell = hex_map_index(&sim->map, enemy->hex), my_cell = hex_map_index(&sim->map, mine->hex);

    /* A skill number of a pack and an officer that is not 周瑜: the code asks only for the trait. */
    mine->officers[1] = 2;
    unit_clear_skill(sim, 2);
    unit_give_traits(sim, 2, 150, fire_god);

    sim_start_fire(sim, mine->hex, 3, false, 0, &sink);
    TEST_CHECK(counts, sim->fire[my_cell] == 0, "a fire never starts on the hex of a unit with fire_immune, also when a deputy has the skill");

    sim->fire[my_cell] = 3;
    u32 before = mine->soldiers;
    sim_run_fire(sim, 0, &sink);
    TEST_CHECK(counts, mine->soldiers == before, "a unit with fire_immune on a hex that burns loses no soldiers");
    sim->fire[my_cell] = 0;

    sim_start_fire(sim, enemy->hex, 3, false, 0, &sink);
    before = enemy->soldiers;
    sim_run_fire(sim, 0, &sink);
    u32 plain_loss = before - enemy->soldiers;
    TEST_CHECK(counts, plain_loss == 500, "a plain fire takes fire.damage_percent of the soldiers");
    TEST_CHECK(counts, enemy->state == UNIT_STATE_NONE, "fire does not confuse the unit: fire and confusion are independent");

    sim->fire[enemy_cell] = 0;
    enemy->soldiers = 5000;
    sim_start_fire(sim, enemy->hex, 3, true, 0, &sink);
    TEST_CHECK(counts, (sim->fire[enemy_cell] & SIM_FIRE_TURNS_MASK) == 3 && (sim->fire[enemy_cell] & SIM_FIRE_STRONG), "a strong fire keeps its turns and its mark");
    before = enemy->soldiers;
    sim_run_fire(sim, 0, &sink);
    TEST_CHECK(counts, before - enemy->soldiers == 2 * plain_loss, "a fire of a unit with fire_power does two times the damage");
    TEST_CHECK(counts, (sim->fire[enemy_cell] & SIM_FIRE_TURNS_MASK) == 2 && (sim->fire[enemy_cell] & SIM_FIRE_STRONG), "the mark stays while the count goes down");
    sim->fire[enemy_cell] = SIM_FIRE_STRONG | 1;
    sim_run_fire(sim, 0, &sink);
    TEST_CHECK(counts, sim->fire[enemy_cell] == 0, "at count 0 the byte of the hex is 0");

    /* The chance of a fire plot: row 0 of a small plots table. */
    sim->content.plot_count = 2;
    sim->content.plots[0].effect = PLOT_FIRE;
    sim->content.plots[1].effect = PLOT_CONFUSE;
    sim_find_officer(sim, 2)->intelligence = 10;
    sim_find_officer(sim, 1)->intelligence = 100;
    sim_find_officer(sim, 0)->intelligence = 10;
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 0, 1) == 100, "fire_sure: a fire plot always succeeds, also against a higher intellect");
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 1, 1) != 100, "fire_sure does not change the other plots");
    TEST_CHECK(counts, sim_plot_chance(sim, enemy, 0, 0) == 0, "a fire plot against a unit with fire_immune fails");
}

internal void
unit_plot_trait_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_trait_world(arena);
    sim_unit *mine = sim->units;
    sim->content.plot_count = 2;
    sim->content.plots[0].effect = PLOT_CONFUSE;
    sim->content.plots[1].effect = PLOT_FALSE_REPORT;
    sim_find_officer(sim, 0)->intelligence = 80;
    sim_find_officer(sim, 1)->intelligence = 60;
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 0, 1) == 70, "the chance without a skill: plot.base_success plus the difference of intellect");

    /* One row of plot_skills.tsv for the skill number 20 of officer 0. */
    unit_give_traits(sim, 0, 20, 0);
    sim->content.plot_skill_count = 1;
    plot_skill *rule = sim->content.plot_skills;
    *rule = { 20, CONTENT_ANY_PLOT, PLOT_SKILL_LOWER_INTELLECT, PLOT_SKILL_SURE };
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 0, 1) == 100, "the outcome sure: 虛實 against a lower intellect");
    sim_find_officer(sim, 1)->intelligence = 90;
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 0, 1) == 40, "the condition lower_intellect does not hold against a higher intellect");
    *rule = { 20, 1, PLOT_SKILL_ALWAYS, PLOT_SKILL_CRITICAL };
    TEST_CHECK(counts, sim_plot_has_outcome(sim, mine, 1, 1, PLOT_SKILL_CRITICAL) && !sim_plot_has_outcome(sim, mine, 0, 1, PLOT_SKILL_CRITICAL),
               "a row with one plot applies only to that plot");
    *rule = { 20, CONTENT_ANY_PLOT, PLOT_SKILL_NO_FEMALE, PLOT_SKILL_DOUBLE };
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 0, 1) == 80, "the outcome double: 傾國 against a unit with no female officer");
    sim_find_officer(sim, 1)->female = true;
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 0, 1) == 40, "傾國 does nothing against a unit with a female officer");
    sim->content.plot_skill_count = 0;

    unit_give_traits(sim, 1, 21, content_trait_bit(TRAIT_SEE_THROUGH_LOWER));
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 0, 1) == 0, "see_through_lower stops a plot from a lower intellect");
    sim_find_officer(sim, 1)->intelligence = 60;
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 0, 1) == 70, "see_through_lower does nothing against a higher intellect");
    unit_give_traits(sim, 1, 21, content_trait_bit(TRAIT_SEE_THROUGH_ALL));
    TEST_CHECK(counts, sim_plot_chance(sim, mine, 0, 1) == 0, "see_through_all stops each plot");

    plot_kind plot = {};
    plot.morale = 15;
    TEST_CHECK(counts, sim_plot_morale(sim, mine, &plot) == 15, "a plot costs its morale");
    unit_give_traits(sim, 0, 22, content_trait_bit(TRAIT_PLOT_THRIFT));
    TEST_CHECK(counts, sim_plot_morale(sim, mine, &plot) == 14, "plot_thrift: a plot costs 1 less morale");
}

/* The states have their own traits: one trait stops one state. */
internal void
unit_state_trait_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_trait_world(arena);
    sim_event buffer[32];
    sim_event_sink sink = { buffer, 0, ArrayCount(buffer) };
    sim_unit *unit = sim->units;
    unit_give_traits(sim, 0, 30, content_trait_bit(TRAIT_NO_CONFUSION));
    sim_set_unit_state(sim, 0, UNIT_STATE_CONFUSED, 2, 0, &sink);
    b32 stopped = unit->state == UNIT_STATE_NONE;
    sim_set_unit_state(sim, 0, UNIT_STATE_FALSE_REPORT, 2, 0, &sink);
    TEST_CHECK(counts, stopped && unit->state == UNIT_STATE_FALSE_REPORT, "no_confusion stops 混亂 and not 偽報");
    unit->state = UNIT_STATE_NONE;
    unit_give_traits(sim, 0, 30, content_trait_bit(TRAIT_NO_CONFUSION) | content_trait_bit(TRAIT_NO_FALSE_REPORT));
    sim_set_unit_state(sim, 0, UNIT_STATE_FALSE_REPORT, 2, 0, &sink);
    TEST_CHECK(counts, unit->state == UNIT_STATE_NONE, "one skill can have the two traits, as 明鏡 has");
}

/* The traits that change one strike. */
internal void
unit_strike_trait_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_trait_world(arena);
    sim_event buffer[64];
    sim_event_sink sink = { buffer, 0, ArrayCount(buffer) };
    sim_unit *mine = sim->units, *enemy = sim->units + 1;
    sim_find_officer(sim, 0)->war = 80;
    sim_find_officer(sim, 1)->war = 60;
    sim_find_officer(sim, 0)->leadership = 80;
    sim_find_officer(sim, 1)->leadership = 60;
    sim_target target = sim_target_at(sim, 0, enemy->hex);

    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_ATTACK, false, false, false, 0, &sink);
    u32 plain = 5000 - enemy->soldiers;
    TEST_CHECK(counts, plain > 0, "a strike takes soldiers");

    /* 藤甲: half of a normal strike, and two times a strike of fire. */
    enemy->soldiers = 5000;
    unit_give_traits(sim, 1, 40, content_trait_bit(TRAIT_RATTAN));
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_ATTACK, false, false, false, 0, &sink);
    u32 rattan_plain = 5000 - enemy->soldiers;
    enemy->soldiers = 5000;
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_TACTIC, false, false, true, 0, &sink);
    u32 rattan_fire = 5000 - enemy->soldiers;
    TEST_CHECK(counts, rattan_plain < plain && rattan_fire > plain, "rattan: less damage from a strike, more from fire");

    enemy->soldiers = 5000;
    unit_give_traits(sim, 1, 40, content_trait_bit(TRAIT_FIRE_IMMUNE));
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_TACTIC, false, false, true, 0, &sink);
    TEST_CHECK(counts, enemy->soldiers == 5000, "fire_immune: a fire arrow does no damage");
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_ATTACK, false, false, false, 0, &sink);
    TEST_CHECK(counts, enemy->soldiers < 5000, "fire_immune does not stop a normal strike");

    enemy->soldiers = 5000;
    unit_clear_skill(sim, 1);
    unit_give_traits(sim, 0, 41, content_trait_bit(TRAIT_FIRE_POWER));
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_TACTIC, false, false, true, 0, &sink);
    TEST_CHECK(counts, 5000 - enemy->soldiers > plain, "fire_power: a fire arrow does more damage");
    unit_clear_skill(sim, 0);

    /* 不屈: few soldiers, no damage with the chance at 100 in this test. */
    enemy->soldiers = 2000;
    unit_give_traits(sim, 1, 42, content_trait_bit(TRAIT_LAST_STAND));
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_ATTACK, false, false, false, 0, &sink);
    TEST_CHECK(counts, enemy->soldiers == 2000, "last_stand: a unit with few soldiers can take no damage");
    enemy->soldiers = 5000;
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_ATTACK, false, false, false, 0, &sink);
    TEST_CHECK(counts, enemy->soldiers < 5000, "last_stand does nothing with many soldiers");

    /* 金剛: a weak attack does nothing. */
    enemy->soldiers = 5000;
    sim->content.rules.skill_weak_attack = plain + 1;
    unit_give_traits(sim, 1, 42, content_trait_bit(TRAIT_WEAK_GUARD));
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_ATTACK, false, false, false, 0, &sink);
    TEST_CHECK(counts, enemy->soldiers == 5000, "weak_guard: an attack under skill.weak_attack can do nothing");
    unit_clear_skill(sim, 1);

    /* 心攻: a part of the losses join the unit. */
    enemy->soldiers = 5000;
    u32 soldiers = mine->soldiers;
    unit_give_traits(sim, 0, 43, content_trait_bit(TRAIT_HEART_ATTACK));
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_ATTACK, false, false, false, 0, &sink);
    TEST_CHECK(counts, mine->soldiers == soldiers + sim_percent_of(5000 - enemy->soldiers, 15), "heart_attack: 15 percent of the losses join the unit");
    unit_clear_skill(sim, 0);

    /* 怒髮: an enemy tactic raises the morale of the target. */
    enemy->soldiers = 5000;
    enemy->morale = 50;
    sim->content.rules.combat_morale_hit = 3;
    unit_give_traits(sim, 1, 44, content_trait_bit(TRAIT_HIT_MORALE));
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_TACTIC, false, false, false, 0, &sink);
    b32 tactic_raises = enemy->morale == 57;
    enemy->morale = 50;
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_ATTACK, false, false, false, 0, &sink);
    TEST_CHECK(counts, tactic_raises && enemy->morale == 47, "hit_morale: a tactic raises the morale, and a normal attack does not");
    unit_clear_skill(sim, 1);

    /* 昂揚: a destroyed target raises the morale of the unit. */
    enemy->soldiers = 1;
    mine->morale = 50;
    unit_give_traits(sim, 0, 45, content_trait_bit(TRAIT_MORALE_ON_DESTROY));
    sim_strike(sim, 0, enemy->hex, &target, 100, CRITICAL_ATTACK, false, false, false, 0, &sink);
    TEST_CHECK(counts, !enemy->alive && mine->morale == 60, "morale_on_destroy: the morale rises when the unit destroys its target");
}

/* The traits of the movement: land and water have their own traits. */
internal void
unit_move_trait_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_trait_world(arena);
    unit_type_kind *type = sim_unit_type(sim, 0);
    u32 base = sim_unit_move(sim, 0, sim->units[0].officers, type);
    sim->content.rules.move_bonus = 4;
    unit_give_traits(sim, 0, 50, content_trait_bit(TRAIT_MOVE_LAND));
    TEST_CHECK(counts, sim_unit_move(sim, 0, sim->units[0].officers, type) == base + 4, "move_land adds move.bonus for a land class");
    unit_give_traits(sim, 0, 50, content_trait_bit(TRAIT_MOVE_WATER));
    TEST_CHECK(counts, sim_unit_move(sim, 0, sim->units[0].officers, type) == base, "move_water adds nothing to a land class on land");
}

/* A world that accepts commands: force 0 acts, unit 2 of force 0 and unit 3 of
   force 1 stand next to unit 1. */
internal sim_state *
unit_trait_battle(memory_arena *arena)
{
    sim_state *sim = unit_trait_world(arena);
    sim->has_content = true;
    sim->turns.order_count = 2;
    sim->turns.order[0].force_id = 0;
    sim->turns.order[1].force_id = 1;
    sim->content.unit_types[0].can_attack = true;
    sim->content.unit_types[0].counterattack = true;
    officer_component extra = *sim_find_officer(sim, 2);
    extra.officer_id = 3;
    sim_add_officer(sim, extra);
    sim->unit_count = 4;
    hex_coord target = sim->units[1].hex;
    for (u32 index = 2; index < 4; ++index)
    {
        sim_unit *unit = sim->units + index;
        *unit = {};
        unit->alive = true; unit->force_id = (u8)(index - 2); unit->unit_type = 0; unit->soldiers = 5000; unit->morale = 50;
        unit->hex = hex_neighbor(target, index == 2 ? 5 : 1);
        unit->officers[0] = (u16)index; unit->officers[1] = SIM_NO_OFFICER; unit->officers[2] = SIM_NO_OFFICER;
        unit_clear_skill(sim, (u16)index);
    }
    for (u16 id = 0; id < 4; ++id)
    {
        sim_find_officer(sim, id)->war = 70;
        sim_find_officer(sim, id)->leadership = 70;
        sim_find_officer(sim, id)->intelligence = 50;
    }
    return sim;
}

internal sim_command
unit_trait_order(sim_state *sim, u32 unit_id, u32 kind, hex_coord to)
{
    sim_command command = {};
    command.unit.unit = (u16)unit_id;
    command.unit.kind = (u8)kind;
    command.unit.to   = to;
    sim->units[unit_id].acted_turn = 0;
    return command;
}

internal void
unit_attack_trait_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_trait_battle(arena);
    sim_event buffer[128];
    sim_event_sink sink = { buffer, 0, ArrayCount(buffer) };
    sim_unit *enemy = sim->units + 1;
    sim_attack(sim, unit_trait_order(sim, 0, 0, enemy->hex), &sink);
    u32 single = 5000 - enemy->soldiers;
    TEST_CHECK(counts, single > 0 && sim->units[0].soldiers < 5000, "a normal attack takes soldiers, and the target answers");

    enemy->soldiers = 5000;
    unit_give_traits(sim, 0, 60, content_trait_bit(TRAIT_DOUBLE_ATTACK));
    sim_attack(sim, unit_trait_order(sim, 0, 0, enemy->hex), &sink);
    TEST_CHECK(counts, 5000 - enemy->soldiers > single, "double_attack: the unit attacks two times, with the chance at 100 in this test");
    unit_clear_skill(sim, 0);

    enemy->soldiers = 5000;
    unit_give_traits(sim, 0, 60, content_trait_bit(TRAIT_AIDE));
    sim->units[2].acted_turn = 0;
    sim_attack(sim, unit_trait_order(sim, 0, 0, enemy->hex), &sink);
    TEST_CHECK(counts, 5000 - enemy->soldiers > single && sim->units[2].acted_turn == 0, "aide: an own unit next to the target also strikes, and it keeps its turn");
    unit_clear_skill(sim, 0);

    /* The joint attack: the target answers only with joint_guard. */
    enemy->soldiers = 5000;
    sim->units[0].soldiers = 5000;
    sim_joint_attack(sim, unit_trait_order(sim, 0, 0, enemy->hex), &sink);
    b32 no_answer = sim->units[0].soldiers == 5000 && enemy->state == UNIT_STATE_NONE;
    enemy->soldiers = 5000;
    sim->units[2].acted_turn = 0;
    unit_give_traits(sim, 1, 61, content_trait_bit(TRAIT_JOINT_GUARD));
    sim_joint_attack(sim, unit_trait_order(sim, 0, 0, enemy->hex), &sink);
    TEST_CHECK(counts, no_answer && sim->units[0].soldiers < 5000, "joint_guard: a joint attack counts as a normal attack, so the target answers");
    unit_clear_skill(sim, 1);

    enemy->soldiers = 5000;
    sim->units[2].acted_turn = 0;
    unit_give_traits(sim, 2, 62, content_trait_bit(TRAIT_JOINT_CONFUSE));
    sim_joint_attack(sim, unit_trait_order(sim, 0, 0, enemy->hex), &sink);
    TEST_CHECK(counts, enemy->state == UNIT_STATE_CONFUSED, "joint_confuse: a partner with the trait is sufficient to confuse the target");
    enemy->state = UNIT_STATE_NONE;
    unit_clear_skill(sim, 2);

    /* A tactic that moves the target can injure its commander. */
    officer_component *foe = sim_find_officer(sim, 1);
    foe->health = 0;
    unit_give_traits(sim, 0, 63, content_trait_bit(TRAIT_PUSH_INJURES));
    sim_push_injury(sim, 0, 1, 0, &sink);
    TEST_CHECK(counts, foe->health == 1, "push_injures: the commander of the moved unit is injured");
    unit_clear_skill(sim, 0);

    /* 射程: only a siege class that shoots gets the range. */
    sim->content.unit_types[0].aptitude = OFFICER_APTITUDE_SIEGE;
    sim->content.unit_types[0].range = 2;
    u32 plain_range = sim_attack_range(sim, sim->units);
    unit_give_traits(sim, 0, 65, content_trait_bit(TRAIT_SIEGE_RANGE));
    TEST_CHECK(counts, sim_attack_range(sim, sim->units) == plain_range + 1, "siege_range: a catapult shoots 1 hex farther");
}

internal void
unit_plot_command_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_trait_battle(arena);
    sim_event buffer[128];
    sim_event_sink sink = { buffer, 0, ArrayCount(buffer) };
    sim_unit *mine = sim->units, *enemy = sim->units + 1, *beside = sim->units + 3;
    sim->content.plot_count = 1;
    plot_kind *plot = sim->content.plots;
    plot->effect = PLOT_CONFUSE; plot->target = PLOT_TARGET_ENEMY; plot->range = 2; plot->turns = 2; plot->from_terrain = CONTENT_NO_TERRAIN;
    /* The skill number 70 always succeeds: the tests do not roll. */
    unit_give_traits(sim, 0, 70, content_trait_bit(TRAIT_PLOT_CHAIN));
    sim->content.plot_skill_count = 1;
    sim->content.plot_skills[0] = { 70, CONTENT_ANY_PLOT, PLOT_SKILL_ALWAYS, PLOT_SKILL_SURE };

    sim_use_plot(sim, unit_trait_order(sim, 0, 0, enemy->hex), &sink);
    TEST_CHECK(counts, enemy->state == UNIT_STATE_CONFUSED && beside->state == UNIT_STATE_CONFUSED && sim->units[2].state == UNIT_STATE_NONE,
               "plot_chain: the plot also hits the enemy next to the target, and no own unit");

    enemy->state = beside->state = UNIT_STATE_NONE;
    sim->content.skills[69].traits = 0;
    unit_give_traits(sim, 1, 71, content_trait_bit(TRAIT_PLOT_RETURN));
    sim_use_plot(sim, unit_trait_order(sim, 0, 0, enemy->hex), &sink);
    TEST_CHECK(counts, enemy->state == UNIT_STATE_CONFUSED && mine->state == UNIT_STATE_CONFUSED, "plot_return: the same plot goes back to the unit that used it");
    mine->state = enemy->state = UNIT_STATE_NONE;
    unit_clear_skill(sim, 1);

    /* The outcome critical: the same plot takes more morale. */
    sim->content.plot_skill_count = 2;
    sim->content.plot_skills[1] = { 70, CONTENT_ANY_PLOT, PLOT_SKILL_ALWAYS, PLOT_SKILL_CRITICAL };
    enemy->morale = 50;
    sim_use_plot(sim, unit_trait_order(sim, 0, 0, enemy->hex), &sink);
    TEST_CHECK(counts, enemy->state == UNIT_STATE_CONFUSED && enemy->morale == 40, "a critical plot takes plot.critical_morale from the target");
}

/* The traits that act outside a battle. */
internal void
unit_other_trait_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_trait_battle(arena);
    sim_event buffer[128];
    sim_event_sink sink = { buffer, 0, ArrayCount(buffer) };
    sim_unit *mine = sim->units, *enemy = sim->units + 1;

    /* 強奪: the item of an officer of the destroyed unit leaves the officer. */
    sim->items[0].exists = true;
    sim->items[0].owner  = 1;
    sim_find_officer(sim, 1)->force_id = 1;
    unit_give_traits(sim, 0, 80, content_trait_bit(TRAIT_PLUNDER_ITEMS));
    sim_capture_officers(sim, 1, 0, false, 0, &sink);
    TEST_CHECK(counts, sim->items[0].owner == SIM_NO_OFFICER, "plunder_items: the unit takes the items of the officers of a destroyed unit");
    unit_clear_skill(sim, 0);
    enemy->alive = false;

    /* 屯田: no grain next to an own base. */
    sim->content.rules.unit_grain_soldiers = 10;
    sim->content.base_count = 1;
    sim->bases[0].exists = true;
    sim->bases[0].force  = 0;
    sim->bases[0].hex    = hex_neighbor(mine->hex, 3);
    mine->grain = 1000;
    sim_run_units_turn(sim, 0, &sink);
    b32 eats = mine->grain < 1000;
    mine->grain = 1000;
    unit_give_traits(sim, 0, 81, content_trait_bit(TRAIT_BASE_NO_GRAIN));
    sim_run_units_turn(sim, 0, &sink);
    b32 keeps = mine->grain == 1000;
    sim->bases[0].force = 1;
    sim_run_units_turn(sim, 0, &sink);
    TEST_CHECK(counts, eats && keeps && mine->grain < 1000, "base_no_grain: the unit uses no grain next to an own base, and uses grain next to another base");
    unit_clear_skill(sim, 0);

    /* 親烏: the table gives the region, and without the table the skill works in each city. */
    city_component city = {};
    city.city_id = 4;
    city.owner_force_id = 0;
    sim_add_city(sim, city);
    officer_component *friend_officer = sim_find_officer(sim, 3);
    friend_officer->force_id = 0;
    friend_officer->current_position = 4;
    b32 none = sim_city_bandit_friend(sim, 4);
    unit_give_traits(sim, 3, 92, content_trait_bit(TRAIT_BANDIT_FRIEND));
    b32 anywhere = sim_city_bandit_friend(sim, 4);
    sim->content.bandit_friend_count = 1;
    sim->content.bandit_friends[0] = { 9, 92 };
    b32 other_region = sim_city_bandit_friend(sim, 4);
    sim->content.bandit_friends[0] = { 4, 92 };
    TEST_CHECK(counts, !none && anywhere && !other_region && sim_city_bandit_friend(sim, 4),
               "bandit_friend: a pack without bandit_regions.tsv: each city; a pack with the table: only the cities of its row");
}

internal void
unit_skill_trait_tests(test_counts *counts, memory_arena *arena)
{
    unit_attack_trait_tests(counts, arena);
    arena_reset(arena);
    unit_plot_command_tests(counts, arena);
    arena_reset(arena);
    unit_other_trait_tests(counts, arena);
    arena_reset(arena);
    unit_trait_table_tests(counts, arena);
    arena_reset(arena);
    unit_fire_trait_tests(counts, arena);
    arena_reset(arena);
    unit_plot_trait_tests(counts, arena);
    arena_reset(arena);
    unit_state_trait_tests(counts, arena);
    arena_reset(arena);
    unit_strike_trait_tests(counts, arena);
    arena_reset(arena);
    unit_move_trait_tests(counts, arena);
}
