/* Unit tests of the authorities of the church and the caliphs (docs/EUROPE.md, E6).
   The loader of authorities.tsv, the rule legitimacy.model, the recognition
   of a faction, the rank that needs it, the monthly rule, the two commands,
   the three effects of history.tsv and the rule of the computer.
   The tests build a small world with no files. Force 0 owns Rome (city 0).
   Force 1 is a Latin kingdom with four cities. Force 2 is a Muslim faction.
   The authority "abbasid" has no seat on the map. */

#define UNIT_AUTHORITY_HEADER "key\tseat\tchurch\tholder\n"
#define UNIT_HISTORY_HEADER   "id\tkey\tname_zh\tchance\tconditions\teffects\tnames_zh\tnames\tstatus\n"

internal sim_state *
unit_authority_world(memory_arena *arena)
{
    sim_state *sim = unit_turn_sim(arena);
    sim->has_content = true;
    sim_rules *rules = &sim->content.rules;
    rules->legitimacy_model  = LEGITIMACY_AUTHORITIES;
    rules->authority_technique = 50;
    rules->authority_loyalty = 1;
    rules->crown_friendliness = 60;
    rules->interdict_months  = 12;
    rules->interdict_loyalty = 2;
    rules->interdict_stop    = 80;
    sim->content.rank_count = 3;
    sim->content.ranks[1].cities = 2;
    sim->content.ranks[2].cities = 4;
    sim->content.ranks[2].needs_emperor = true;
    sim->content.authority_count = 2;
    snprintf(sim->content.authorities[0].key, CONTENT_KEY_SIZE, "rome");
    sim->content.authorities[0].seat_city = 0;
    sim->content.authorities[0].church    = CONTENT_CHURCH_LATIN;
    snprintf(sim->content.authorities[1].key, CONTENT_KEY_SIZE, "abbasid");
    sim->content.authorities[1].seat_city = CONTENT_NO_SEAT;
    sim->content.authorities[1].church    = CONTENT_CHURCH_MUSLIM;
    sim->content.force_authority[0] = 1;
    sim->content.force_authority[1] = 1;
    sim->content.force_authority[2] = 2;
    for (u8 city = 0; city < 8; ++city) sim_add_city(sim, unit_city(city, SIM_NO_FORCE, 1000));
    for (u8 city = 0; city < 5; ++city) sim_find_city(sim, city)->owner_force_id = city == 0 ? 0 : 1;   /* Rome, then four cities of force 1 */
    sim_find_city(sim, 0)->owner_force_id = 0;
    sim_find_city(sim, 5)->owner_force_id = 2;
    for (u8 force = 0; force < 3; ++force)
    {
        sim_add_officer(sim, unit_officer((u16)(10 + force), force, OFFICER_STATUS_RULER, force == 0 ? 0 : force == 1 ? 1 : 5, 70, 70, 70, 70, 100));
        sim_add_officer(sim, unit_officer((u16)(20 + force), force, OFFICER_STATUS_REGULAR_OFFICER, force == 0 ? 0 : force == 1 ? 1 : 5, 50, 50, 50, 50, 100));
        sim_find_officer(sim, (u16)(20 + force))->loyalty = 80;
    }
    sim->turns.order_count = 3;
    for (u8 force = 0; force < 3; ++force) sim->turns.order[force].force_id = force;
    sim->turns.acting = 1;   /* the turn of force 1 */
    return sim;
}

/* Sets the friendliness between two factions in the two directions. */
internal void
unit_authority_friend(sim_state *sim, u8 a, u8 b, u8 friendliness)
{
    sim->diplomacy.friendliness[a][b] = friendliness;
    sim->diplomacy.friendliness[b][a] = friendliness;
}

internal void
unit_authority_loader_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table cities = {}, table = {};
    u32 bad_row = 0;
    tsv_parse(content_test_text(arena, "id\tkey\n0\trome\n1\tbaghdad\n"), arena, &cities);
    char *good = content_test_text(arena, UNIT_AUTHORITY_HEADER "rome\trome\tlatin\tInnocent III\nabbasid\tbaghdad\tmuslim\tal-Nasir\nalmohad\t-\tmuslim\tMuhammad\n");
    TEST_CHECK(counts, tsv_parse(good, arena, &table) && content_load_authorities(&content, &table, &cities, &bad_row) && content.authority_count == 3 &&
                       content.authorities[0].seat_city == 0 && content.authorities[1].seat_city == 1 && content.authorities[2].seat_city == CONTENT_NO_SEAT &&
                       content.authorities[0].church == CONTENT_CHURCH_LATIN && strcmp(content.authorities[1].holder, "al-Nasir") == 0,
               "authorities.tsv: the key, the seat city, the church and the holder; a dash is a seat that is not on the map");
    TEST_CHECK(counts, content_find_authority(&content, "abbasid") == 1 && content_find_authority(&content, "pope") == CONTENT_AUTHORITY_CAPACITY,
               "an authority is found by its key");

    content = {};
    tsv_table bad = {};
    char *no_city = content_test_text(arena, UNIT_AUTHORITY_HEADER "rome\tparis\tlatin\tx\n");
    TEST_CHECK(counts, tsv_parse(no_city, arena, &bad) && !content_load_authorities(&content, &bad, &cities, &bad_row) && content.authority_count == 0,
               "an authority with a seat that cities.tsv lacks is a bad row");
    char *no_church = content_test_text(arena, UNIT_AUTHORITY_HEADER "rome\trome\tpagan\tx\n");
    TEST_CHECK(counts, tsv_parse(no_church, arena, &bad) && !content_load_authorities(&content, &bad, &cities, &bad_row), "an authority with an unknown church is a bad row");
    char *twice = content_test_text(arena, UNIT_AUTHORITY_HEADER "rome\trome\tlatin\tx\nrome\t-\tlatin\ty\n");
    TEST_CHECK(counts, tsv_parse(twice, arena, &bad) && !content_load_authorities(&content, &bad, &cities, &bad_row), "a key that two rows use is a bad row");

    tsv_table rules = {};
    tsv_parse(content_test_text(arena, "key\tvalue\tstatus\nlegitimacy.model\tauthorities\tassumed\ncrown.friendliness\t60\tassumed\ninterdict.months\t12\tassumed\n"), arena, &rules);
    TEST_CHECK(counts, content_load_rules(&content, &rules, &bad_row) && content.rules.legitimacy_model == LEGITIMACY_AUTHORITIES &&
                       content.rules.crown_friendliness == 60 && content.rules.interdict_months == 12,
               "rules.tsv: legitimacy.model authorities and the numbers of the crown and the interdict");
    tsv_parse(content_test_text(arena, "key\tvalue\tstatus\nlegitimacy.model\temperor\tassumed\n"), arena, &rules);
    TEST_CHECK(counts, content_load_rules(&content, &rules, &bad_row) && content.rules.legitimacy_model == LEGITIMACY_EMPEROR, "legitimacy.model emperor is the model of the Han emperor");
    tsv_parse(content_test_text(arena, "key\tvalue\tstatus\nlegitimacy.model\tpope\tassumed\n"), arena, &rules);
    TEST_CHECK(counts, !content_load_rules(&content, &rules, &bad_row), "an unknown value of legitimacy.model is a bad row");
}

internal void
unit_authority_history_row_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    u32 bad_row = 0;
    const char *good[] =
    {
        "0\ta\tx\t100\tdate_from:1200/1\tsee_holder:rome:latin\t-\t-\tx\n",
        "0\ta\tx\t100\tdate_from:1200/1\tsee_holder:antioch:-\t-\t-\tx\n",
        "0\ta\tx\t100\tdate_from:1200/1\tauthority_seat:abbasid:4\t-\t-\tx\n",
        "0\ta\tx\t100\topening\tinterdict:8\t-\t-\tx\n",
    };
    for (u32 index = 0; index < ArrayCount(good); ++index)
    {
        tsv_table table = {};
        char name[96];
        snprintf(name, sizeof(name), "history.tsv: the effect of the E6 row %u loads", index);
        char *full = (char *)arena_push_size(arena, 512);
        snprintf(full, 512, "%s%s", UNIT_HISTORY_HEADER, good[index]);
        TEST_CHECK(counts, tsv_parse(full, arena, &table) && content_load_history(&content, &table, &bad_row), name);
    }
    const char *bad[] =
    {
        "0\ta\tx\t100\topening\tsee_holder:rome:pagan\t-\t-\tx\n",
        "0\ta\tx\t100\topening\tsee_holder:rome\t-\t-\tx\n",
        "0\ta\tx\t100\topening\tauthority_seat:abbasid:200\t-\t-\tx\n",
        "0\ta\tx\t100\topening\tinterdict:0\t-\t-\tx\n",
        "0\ta\tx\t100\topening\tinterdict:99999\t-\t-\tx\n",
    };
    for (u32 index = 0; index < ArrayCount(bad); ++index)
    {
        tsv_table table = {};
        char name[96];
        snprintf(name, sizeof(name), "history.tsv: the bad E6 effect %u is a bad row", index);
        char *full = (char *)arena_push_size(arena, 512);
        snprintf(full, 512, "%s%s", UNIT_HISTORY_HEADER, bad[index]);
        TEST_CHECK(counts, tsv_parse(full, arena, &table) && !content_load_history(&content, &table, &bad_row), name);
    }
}

internal void
unit_authority_recognition_tests(test_counts *counts, memory_arena *arena)
{
    u64 mark = arena->used;
    sim_state *sim = unit_authority_world(arena);
    TEST_CHECK(counts, sim_uses_authorities(sim) && sim_authority_of(sim, 1) == 0 && sim_authority_of(sim, 2) == 1 && sim_authority_of(sim, 7) == CONTENT_AUTHORITY_CAPACITY,
               "a pack with the model authorities uses them, and each faction has its authority");
    TEST_CHECK(counts, sim_authority_keeper(sim, 0) == 0 && sim_authority_seat(sim, 0) == 0 && sim_authority_seat(sim, 1) == SIM_NO_CITY,
               "the owner of the seat is the keeper, and a seat that is not on the map has no city");
    TEST_CHECK(counts, sim_authority_keeper(sim, 1) == 2, "a seat that is not on the map has the largest faction of the authority as keeper");
    TEST_CHECK(counts, sim_has_recognition(sim, 0), "the keeper of a seat has recognition");
    TEST_CHECK(counts, !sim_has_recognition(sim, 1), "a faction with no alliance and no crown has no recognition");
    sim->diplomacy.relation[0][1] = sim->diplomacy.relation[1][0] = RELATION_ALLIANCE;
    TEST_CHECK(counts, sim_has_recognition(sim, 1), "an ally of the keeper has recognition");
    sim->diplomacy.relation[0][1] = sim->diplomacy.relation[1][0] = RELATION_NONE;
    sim->authorities.crown[1] = 1;
    TEST_CHECK(counts, sim_has_recognition(sim, 1), "a faction that has a crown has recognition");
    sim->authorities.interdict[1] = 3;
    TEST_CHECK(counts, !sim_has_recognition(sim, 1), "an interdict takes the recognition away");
    sim->authorities.interdict[1] = 0;
    sim->content.rules.legitimacy_model = LEGITIMACY_EMPEROR;
    TEST_CHECK(counts, !sim_uses_authorities(sim) && !sim_has_recognition(sim, 0), "the model emperor has no authority and no recognition");
    arena->used = mark;
}

internal void
unit_authority_rank_tests(test_counts *counts, memory_arena *arena)
{
    u64 mark = arena->used;
    sim_state *sim = unit_authority_world(arena);
    sim_find_city(sim, 6)->owner_force_id = 1;   /* force 1 has five cities */
    TEST_CHECK(counts, sim_faction_city_count(sim, 1) == 5, "the test faction has five cities");
    TEST_CHECK(counts, sim_rank_for(sim, 1, false) == 1, "a faction with the cities for a king and no recognition gets the rank below");
    sim->authorities.crown[1] = 1;
    TEST_CHECK(counts, sim_rank_for(sim, 1, false) == 2, "a faction with recognition gets the rank of king");
    sim->authorities.crown[1] = 0;
    TEST_CHECK(counts, sim_rank_for(sim, 1, true) == 2, "at the start of a game the ruler keeps the rank that his cities give");
    sim->rank[1] = 2;
    TEST_CHECK(counts, sim_rank_for(sim, 1, false) == 2, "a ruler who holds the rank of king keeps it with no recognition");
    sim->rank[1] = 0;
    sim->content.rules.legitimacy_model = LEGITIMACY_EMPEROR;
    TEST_CHECK(counts, sim_rank_for(sim, 1, false) == 2, "the model emperor does not look at the authorities when no Han emperor exists");
    arena->used = mark;
}

internal void
unit_authority_month_tests(test_counts *counts, memory_arena *arena)
{
    u64 mark = arena->used;
    sim_state *sim = unit_authority_world(arena);
    sim_find_officer(sim, 20)->han_respect = 2;   /* a devout officer of the keeper of Rome */
    sim_find_officer(sim, 12)->han_respect = 2;   /* a devout ruler gains nothing */
    sim_find_officer(sim, 21)->han_respect = 2;   /* a devout officer of another faction */
    sim_event_sink sink = sim_sink_from_arena(arena, 64);
    sim_run_authority_month(sim, 0, &sink);
    TEST_CHECK(counts, sim->technique_points[0] == 50 && sim->technique_points[1] == 0 && sim->technique_points[2] == 0,
               "each month the owner of the seat gains the technique points of the rule");
    TEST_CHECK(counts, sim_find_officer(sim, 20)->loyalty == 81 && sim_find_officer(sim, 21)->loyalty == 80 && sim_find_officer(sim, 22)->loyalty == 80,
               "each month the devout officers of the owner gain loyalty, and no other officer does");
    sim_find_city(sim, 0)->owner_force_id = 1;   /* force 1 takes Rome */
    sim_run_authority_month(sim, 0, &sink);
    TEST_CHECK(counts, sim->technique_points[1] == 50 && sim->technique_points[0] == 50 && sim_find_officer(sim, 21)->loyalty == 81,
               "the points follow the owner of the seat");
    sim->authorities.holder[0] = 1;   /* the see has no holder */
    sim_run_authority_month(sim, 0, &sink);
    TEST_CHECK(counts, sim->technique_points[1] == 50, "a seat with no holder gives nothing");
    sim->authorities.holder[0] = 0;
    sim->content.rules.legitimacy_model = LEGITIMACY_EMPEROR;
    sim_run_authority_month(sim, 0, &sink);
    TEST_CHECK(counts, sim->technique_points[1] == 50, "the model emperor gives nothing through the authorities");
    arena->used = mark;
}

internal void
unit_authority_crown_tests(test_counts *counts, memory_arena *arena)
{
    u64 mark = arena->used;
    sim_state *sim = unit_authority_world(arena);
    sim_find_city(sim, 6)->owner_force_id = 1;
    sim_m8_order order = {};
    order.force = 1;
    TEST_CHECK(counts, sim_check_ask_crown(sim, &order).refusal == SIM_REFUSAL_NOT_FRIENDLY, "a ruler with low friendliness asks and the keeper refuses");
    unit_authority_friend(sim, 0, 1, 59);
    TEST_CHECK(counts, sim_check_ask_crown(sim, &order).refusal == SIM_REFUSAL_NOT_FRIENDLY, "one point under crown.friendliness is not enough");
    unit_authority_friend(sim, 0, 1, 60);
    TEST_CHECK(counts, sim_check_ask_crown(sim, &order).refusal == SIM_REFUSAL_NONE, "friendliness of crown.friendliness is enough");
    sim->authorities.interdict[1] = 2;
    TEST_CHECK(counts, sim_check_ask_crown(sim, &order).refusal == SIM_REFUSAL_NOT_FRIENDLY, "a faction under interdict gets no crown");
    sim->authorities.interdict[1] = 0;
    order.force = 0;
    sim->turns.acting = 0;
    TEST_CHECK(counts, sim_check_ask_crown(sim, &order).refusal == SIM_REFUSAL_HAS_RECOGNITION, "the keeper of the seat has recognition and needs no crown");
    order.force = 1;
    sim->turns.acting = 0;
    TEST_CHECK(counts, sim_check_ask_crown(sim, &order).refusal == SIM_REFUSAL_NOT_YOUR_TURN, "a ruler asks in his own turn");
    sim->turns.acting = 1;
    order.force = 2;
    TEST_CHECK(counts, sim_check_ask_crown(sim, &order).refusal == SIM_REFUSAL_NOT_YOUR_TURN, "another faction cannot ask in the turn of force 1");

    order.force = 1;
    sim_event_sink sink = sim_sink_from_arena(arena, 64);
    sim_command command = sim_make_command(sim, SIM_COMMAND_ASK_CROWN);
    command.m8 = order;
    sim_apply(sim, command, &sink);
    u32 crowns = 0, ranks = 0;
    for (u32 index = 0; index < sink.count; ++index)
    {
        crowns += sink.events[index].kind == SIM_EVENT_CROWN_GIVEN && sink.events[index].faction == 1;
        ranks  += sink.events[index].kind == SIM_EVENT_RANK_CHANGED && sink.events[index].faction == 1;
    }
    TEST_CHECK(counts, sim->authorities.crown[1] == 1 && crowns == 1 && sim_has_recognition(sim, 1), "ask_crown gives the crown and one event");
    TEST_CHECK(counts, sim->rank[1] == 2 && ranks == 1, "the crown lifts the rank of king at once");
    TEST_CHECK(counts, sim_check_ask_crown(sim, &order).refusal == SIM_REFUSAL_HAS_RECOGNITION, "a faction with a crown does not ask again");

    sim->content.authority_count = 0;
    TEST_CHECK(counts, sim_check_ask_crown(sim, &order).refusal == SIM_REFUSAL_NO_AUTHORITY, "a pack with no authorities refuses ask_crown");
    arena->used = mark;
}

internal void
unit_authority_interdict_tests(test_counts *counts, memory_arena *arena)
{
    u64 mark = arena->used;
    sim_state *sim = unit_authority_world(arena);
    sim->turns.acting = 0;   /* the turn of the keeper of Rome */
    sim_m8_order order = {};
    order.force = 0;
    order.value = 1;
    TEST_CHECK(counts, sim_check_interdict(sim, &order).refusal == SIM_REFUSAL_NONE, "the owner of Rome can put a faction under interdict");
    order.value = 0;
    TEST_CHECK(counts, sim_check_interdict(sim, &order).refusal == SIM_REFUSAL_NO_TARGET, "a faction cannot put itself under interdict");
    order.value = 7;
    TEST_CHECK(counts, sim_check_interdict(sim, &order).refusal == SIM_REFUSAL_NO_TARGET, "a faction with no ruler is no target");
    order.value = 1;
    unit_authority_friend(sim, 0, 1, 81);
    TEST_CHECK(counts, sim_check_interdict(sim, &order).refusal == SIM_REFUSAL_NO_TARGET, "a friend above interdict.stop is no target");
    unit_authority_friend(sim, 0, 1, 10);
    order.force = 1;
    order.value = 2;
    sim->turns.acting = 1;
    TEST_CHECK(counts, sim_check_interdict(sim, &order).refusal == SIM_REFUSAL_NO_AUTHORITY, "a faction that does not own Rome cannot put an interdict");

    order.force = 0;
    order.value = 1;
    sim->turns.acting = 0;
    sim_event_sink sink = sim_sink_from_arena(arena, 64);
    sim_command command = sim_make_command(sim, SIM_COMMAND_INTERDICT);
    command.m8 = order;
    sim_apply(sim, command, &sink);
    TEST_CHECK(counts, sim->authorities.interdict[1] == 12 && sink.count == 1 && sink.events[0].kind == SIM_EVENT_INTERDICT_STARTED &&
                       sink.events[0].faction == 1 && sink.events[0].detail == 12,
               "interdict sets the months of the rule and gives one event");
    TEST_CHECK(counts, sim_check_interdict(sim, &order).refusal == SIM_REFUSAL_NO_TARGET, "a faction under interdict is not a target again");

    sim_run_authority_month(sim, 0, &sink);
    TEST_CHECK(counts, sim_find_officer(sim, 21)->loyalty == 78 && sim->authorities.interdict[1] == 11,
               "each month of an interdict the officers lose loyalty and the months count down");
    for (u32 month = 0; month < 11; ++month) sim_run_authority_month(sim, 0, &sink);
    u32 lifted = 0;
    for (u32 index = 0; index < sink.count; ++index) lifted += sink.events[index].kind == SIM_EVENT_INTERDICT_LIFTED && sink.events[index].faction == 1;
    TEST_CHECK(counts, sim->authorities.interdict[1] == 0 && lifted == 1, "the interdict ends after its months with one event");

    sim->authorities.interdict[1] = 5;
    unit_authority_friend(sim, 0, 1, 81);
    u32 loyalty = sim_find_officer(sim, 21)->loyalty;
    sim_run_authority_month(sim, 0, &sink);
    TEST_CHECK(counts, sim->authorities.interdict[1] == 0 && sim_find_officer(sim, 21)->loyalty == loyalty, "a faction above interdict.stop leaves the interdict at once, with no loss");
    arena->used = mark;
}

internal void
unit_authority_effect_tests(test_counts *counts, memory_arena *arena)
{
    u64 mark = arena->used;
    sim_state *sim = unit_authority_world(arena);
    sim_authority_see_holder(sim, "rome", "greek");
    TEST_CHECK(counts, sim_authority_church(sim, 0) == CONTENT_CHURCH_GREEK && sim_authority_church(sim, 1) == CONTENT_CHURCH_MUSLIM,
               "see_holder changes the church of one authority");
    sim_authority_see_holder(sim, "rome", "-");
    TEST_CHECK(counts, sim_authority_church(sim, 0) == CONTENT_CHURCH_NONE && sim_authority_keeper(sim, 0) == SIM_NO_FORCE, "see_holder with a dash leaves the see with no holder and no keeper");
    sim_authority_see_holder(sim, "pope", "latin");
    sim_authority_see_holder(sim, "rome", "pagan");
    TEST_CHECK(counts, sim_authority_church(sim, 0) == CONTENT_CHURCH_NONE, "an unknown authority or church changes nothing");

    sim_authority_move_seat(sim, "abbasid", 5);
    TEST_CHECK(counts, sim_authority_seat(sim, 1) == 5 && sim_authority_keeper(sim, 1) == 2, "authority_seat moves a seat to a city, and its owner becomes the keeper");
    sim_authority_move_seat(sim, "abbasid", 200);
    TEST_CHECK(counts, sim_authority_seat(sim, 1) == 5, "authority_seat with a bad city changes nothing");
    sim_authority_move_seat(sim, "rome", 0);
    TEST_CHECK(counts, sim_authority_seat(sim, 0) == 0, "a move to city 0 is a real move");

    sim_event_sink sink = sim_sink_from_arena(arena, 16);
    sim_start_interdict(sim, 1, 0, 8, 0, &sink);
    TEST_CHECK(counts, sim->authorities.interdict[1] == 8 && sink.count == 1 && sink.events[0].detail == 8, "the effect interdict sets the months of the row");
    sim_start_interdict(sim, SIM_NO_FORCE, 0, 8, 0, &sink);
    sim_start_interdict(sim, 2, 0, 0, 0, &sink);
    TEST_CHECK(counts, sink.count == 1 && sim->authorities.interdict[2] == 0, "an interdict of no faction or of 0 months does nothing");
    arena->used = mark;
}

internal void
unit_authority_ai_tests(test_counts *counts, memory_arena *arena)
{
    u64 mark = arena->used;
    sim_state *sim = unit_authority_world(arena);
    sim_find_city(sim, 6)->owner_force_id = 1;
    sim_event_sink sink = sim_sink_from_arena(arena, 64);
    unit_authority_friend(sim, 0, 1, 70);
    sim_ai_authority(sim, 1, &sink);
    TEST_CHECK(counts, sim->authorities.crown[1] == 1 && sim->rank[1] == 2, "the AI asks for a crown when it has the cities and the keeper is friendly");

    sim = unit_authority_world(arena);
    sim_find_city(sim, 6)->owner_force_id = 1;
    unit_authority_friend(sim, 0, 1, 10);
    sim_ai_authority(sim, 1, &sink);
    TEST_CHECK(counts, sim->authorities.crown[1] == 0, "the AI gets no crown from an unfriendly keeper");

    sim = unit_authority_world(arena);
    sim_find_city(sim, 4)->owner_force_id = SIM_NO_FORCE;   /* force 1 has three cities, and the rank of king needs four */
    unit_authority_friend(sim, 0, 1, 70);
    sim_ai_authority(sim, 1, &sink);
    TEST_CHECK(counts, sim->authorities.crown[1] == 0, "the AI with too few cities does not ask");

    sim = unit_authority_world(arena);
    sim->turns.acting = 0;
    sim->diplomacy.relation[0][2] = sim->diplomacy.relation[2][0] = RELATION_ALLIANCE;   /* Rome allies with force 2, and force 1 is at war with it */
    unit_authority_friend(sim, 0, 1, 10);
    sim_ai_authority(sim, 0, &sink);
    TEST_CHECK(counts, sim->authorities.interdict[1] == 12, "the owner of Rome puts under interdict a faction at war with an ally");
    sim_ai_authority(sim, 0, &sink);
    TEST_CHECK(counts, sim->authorities.interdict[1] == 12 && sim->authorities.interdict[2] == 0, "the AI does not put the same faction under interdict twice");

    sim = unit_authority_world(arena);
    sim->turns.acting = 0;
    unit_authority_friend(sim, 0, 1, 10);
    sim_ai_authority(sim, 0, &sink);
    TEST_CHECK(counts, sim->authorities.interdict[1] == 0 && sim->authorities.interdict[2] == 0, "the owner of Rome with no allied victim puts no interdict");
    arena->used = mark;
}

internal void
unit_authority_tests(test_counts *counts, memory_arena *arena)
{
    unit_authority_loader_tests(counts, arena);
    unit_authority_history_row_tests(counts, arena);
    unit_authority_recognition_tests(counts, arena);
    unit_authority_rank_tests(counts, arena);
    unit_authority_month_tests(counts, arena);
    unit_authority_crown_tests(counts, arena);
    unit_authority_interdict_tests(counts, arena);
    unit_authority_effect_tests(counts, arena);
    unit_authority_ai_tests(counts, arena);
}
