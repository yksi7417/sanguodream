/* Feature tests of M7 court (officers-and-relations.md, factions-corps-and-
   posts.md, city-commands.md, diplomacy.md, duel.md, debate.md; F-051 to
   F-059, F-100, F-101, F-110 to F-113), through engine_api with the real
   engine and the sample save: the tables, the state of an officer, 褒賞,
   官職, 登庸, 授與, 仲介, 流放, the diplomacy commands, the plots of
   factions, the duel, the debate and the loyalty of a month. The costs are
   documented; the chances and amounts are assumed (Q-028, Q-029, Q-037 to
   Q-039, Q-177 to Q-181). */

#define FEATURE_POST_REGISTRAR 64   /* 主簿: merit 4000, 6000 soldiers, politics +1 */
#define FEATURE_POST_CHANCELLOR 0   /* 丞相: merit 36000, rank 7 */
#define FEATURE_SKILL_DEBATER  87   /* 論客 */

internal engine_officer_edit
feature_edit_of(engine_api *api, u16 officer_id)
{
    engine_officer_detail detail = api->officer_detail(api->engine, officer_id);
    engine_officer officer = {};
    feature_officer_by_id(api, officer_id, &officer);
    return engine_officer_edit_of(&detail, &officer);
}

internal u8
feature_officer_force(engine_api *api, u16 officer_id)
{
    engine_officer officer = {};
    return feature_officer_by_id(api, officer_id, &officer) ? officer.force_id : (u8)ENGINE_NO_FACTION;
}

/* Another faction that owns a city, and that city. */
internal u8
feature_other_faction(engine_api *api, u8 force, u8 *city_out)
{
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        if (city.owner_force_id != force && city.owner_force_id != ENGINE_NO_FACTION && api->force_ruler(api->engine, city.owner_force_id) != ENGINE_NO_OFFICER)
        {
            if (city_out) *city_out = city.city_id;
            return city.owner_force_id;
        }
    }
    return ENGINE_NO_FACTION;
}

/* An active officer of the faction that is not the ruler, not in a unit,
   and not one of the excluded ids. */
internal u16
feature_find_officer(engine_api *api, u8 force, u16 exclude_a, u16 exclude_b)
{
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.force_id != force || officer.status == OFFICER_STATUS_RULER || officer.status > 3 || officer.current_position >= 87) continue;
        if (officer.officer_id == exclude_a || officer.officer_id == exclude_b) continue;
        return officer.officer_id;
    }
    return ENGINE_NO_OFFICER;
}

internal engine_court_order
feature_court_order(u8 city, u16 officer, u16 target)
{
    engine_court_order order = {};
    order.city_id     = city;
    order.officer     = officer;
    order.target      = target;
    order.item        = 0xFF;
    order.post        = 0xFF;
    order.force       = order.force_2 = ENGINE_NO_FACTION;
    order.target_city = ENGINE_NO_CITY;
    return order;
}

internal engine_check
feature_court_check(engine_api *api, engine_command_kind kind, engine_court_order *order)
{
    engine_command command = {};
    command.kind  = kind;
    command.court = *order;
    return api->can_issue(api->engine, &command);
}

internal void
feature_court_table_tests(test_counts *counts, feature_city_setup *setup)
{
    engine_api *api = setup->api;
    void *e = api->engine;
    engine_content_status status = api->content_status(e);
    TEST_CHECK(counts, api->skill_kind_count(e) == 100 && status.skill_count == 100 && api->post_kind_count(e) == 80 && api->rank_kind_count(e) == 10 &&
                       status.item_count >= 16,
               "skills.tsv has the 100 skills, posts.tsv the 80 posts and ranks.tsv the 10 ranks of the spec");
    engine_skill_kind capture = api->skill_kind_at(e, 18);
    TEST_CHECK(counts, capture.number == 19 && capture.hooked && strcmp(capture.effect, "capture") == 0, "捕縛 #19 drives the capture rule");
    u16 ruler = api->force_ruler(e, setup->force);
    engine_officer_detail ruler_detail = api->officer_detail(e, ruler);
    u8 rank = api->faction_rank(e, setup->force);
    TEST_CHECK(counts, ruler_detail.loyalty == 100 && ruler_detail.soldier_cap == api->rank_kind_at(e, rank).soldiers,
               "a ruler has loyalty 100 and leads the soldiers of its rank");
    u32 cities = 0;
    for (u32 index = 0; index < api->city_count(e); ++index) cities += api->city_at(e, index).owner_force_id == setup->force;
    u8 expected = 0;
    for (u32 index = 0; index < api->rank_kind_count(e); ++index) if (cities >= api->rank_kind_at(e, index).cities) expected = (u8)index;
    TEST_CHECK(counts, rank == expected, "the rank of the ruler follows the city count (ranks.tsv)");
    u16 officer = feature_find_officer(api, setup->force, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    engine_officer_detail detail = api->officer_detail(e, officer);
    TEST_CHECK(counts, detail.loyalty >= 1 && detail.loyalty <= 100 && detail.post == ENGINE_NO_ROW && detail.soldier_cap == 5000 && detail.stamina == 100,
               "an officer starts with the loyalty of the save (G2-035), no post, 5000 soldiers and full stamina");
    u32 items = 0, known = 0;
    for (u32 index = 0; index < api->item_count(e); ++index)
    {
        engine_item item = api->item_at(e, index);
        if (item.id == ENGINE_NO_ROW) continue;
        items++;
        known += item.key[0] != 0;
    }
    TEST_CHECK(counts, items > 0 && known > 0, "the items of the save load, and items.tsv knows the kind of some by the Big5 name");
}

/* Up to count officers of the force that the faction can reward now. The
   editor sets their loyalty to 50 first. */
internal u32
feature_reward_candidates(engine_api *api, u8 force, u16 *out, u32 count)
{
    void *e = api->engine;
    u32 found = 0;
    for (u32 index = 0; index < api->officer_count(e) && found < count; ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        if (officer.force_id != force || officer.status == OFFICER_STATUS_RULER || officer.status > 3 || officer.current_position >= 87) continue;
        engine_officer_edit edit = feature_edit_of(api, officer.officer_id);
        edit.loyalty = 50;
        api->set_officer(e, officer.officer_id, &edit);
        if (api->officer_detail(e, officer.officer_id).reward_rise == 0) continue;
        out[found++] = officer.officer_id;
    }
    return found;
}

/* The rise that rules.tsv of the placeholder pack gives: 10 less one for
   each 15 of the distance of compatibility to the ruler, at least 1. */
internal u32
feature_expected_rise(engine_api *api, u8 force, u16 officer_id)
{
    engine_officer officer = {}, ruler = {};
    feature_officer_by_id(api, officer_id, &officer);
    feature_officer_by_id(api, api->force_ruler(api->engine, force), &ruler);
    u32 distance = officer.compatibility > ruler.compatibility ? officer.compatibility - ruler.compatibility : ruler.compatibility - officer.compatibility;
    distance %= 150;
    if (distance > 75) distance = 150 - distance;
    u32 penalty = distance / 15;
    return penalty >= 10 ? 1 : 10 - penalty;
}

internal void
feature_reward_and_post_tests(test_counts *counts, feature_city_setup *setup)
{
    engine_api *api = setup->api;
    void *e = api->engine;

    /* 褒賞 of a set of the whole force (G2-040) */
    u16 set[4];
    u32 count = feature_reward_candidates(api, setup->force, set, 4);
    engine_court_order order = feature_court_order(setup->city, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    u32 rises[4] = {}, total = 0;
    b32 formula = true;
    for (u32 place = 0; place < count; ++place)
    {
        order.rewards[place] = set[place];
        rises[place] = api->officer_detail(e, set[place]).reward_rise;
        formula = formula && rises[place] == feature_expected_rise(api, setup->force, set[place]);
        total += rises[place];
    }
    order.reward_count = count;
    engine_check check = feature_court_check(api, ENGINE_COMMAND_REWARD, &order);
    u32 gold = api->city_at(e, setup->city).gold;
    engine_command_result result = api->command_court(e, ENGINE_COMMAND_REWARD, &order);
    b32 each_rose = true;
    for (u32 place = 0; place < count; ++place)
    {
        engine_officer_detail detail = api->officer_detail(e, set[place]);
        each_rose = each_rose && detail.loyalty == 50 + rises[place] && detail.reward_rise == 0;
    }
    TEST_CHECK(counts, count == 4 && check.refusal == SIM_REFUSAL_NONE && check.gold == 400 && check.action_points == 20 && check.quantity == total &&
                       result.refusal == SIM_REFUSAL_NONE && api->city_at(e, setup->city).gold == gold - 400 && each_rose,
               "褒賞 (G2-040): one order rewards four officers of the force for 100 gold and 5 action points each; each loyalty rises by the rise of the list");
    TEST_CHECK(counts, formula, "the rise is reward.loyalty less the distance of compatibility to the ruler over reward.compatibility_divisor, at least reward.loyalty_min (assumed, Q-028)");
    TEST_CHECK(counts, api->command_court(e, ENGINE_COMMAND_REWARD, &order).refusal == SIM_REFUSAL_LIMIT_REACHED, "refused: one reward for each officer in each turn");
    engine_officer_edit full = feature_edit_of(api, set[0]);
    full.loyalty = 100;
    api->set_officer(e, set[0], &full);
    order.reward_count = 1;
    TEST_CHECK(counts, api->officer_detail(e, set[0]).reward_rise == 0 && feature_court_check(api, ENGINE_COMMAND_REWARD, &order).refusal == SIM_REFUSAL_VALUE_AT_MAXIMUM,
               "refused: an officer with loyalty 100 gets no reward, and the list shows no rise for it");
    order.reward_count = 0;
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_REWARD, &order).refusal == SIM_REFUSAL_NO_OFFICER, "refused: an empty set");
    u16 ids[3];
    feature_take_officers(api, setup->city, ids, 2);

    /* 官職 */
    engine_officer before = {};
    feature_officer_by_id(api, ids[0], &before);
    engine_officer_edit edit = feature_edit_of(api, ids[0]);
    edit.merit = 4000;
    api->set_officer(e, ids[0], &edit);
    engine_court_order appoint = feature_court_order(setup->city, ENGINE_NO_OFFICER, ids[0]);
    appoint.post = FEATURE_POST_REGISTRAR;
    engine_command_result appointed = api->command_court(e, ENGINE_COMMAND_APPOINT, &appoint);
    engine_officer after = {};
    feature_officer_by_id(api, ids[0], &after);
    engine_officer_detail detail = api->officer_detail(e, ids[0]);
    TEST_CHECK(counts, appointed.refusal == SIM_REFUSAL_NONE && detail.post == FEATURE_POST_REGISTRAR && detail.soldier_cap == 6000 &&
                       after.politics == before.politics + 1 && api->post_holder(e, setup->force, FEATURE_POST_REGISTRAR) == ids[0],
               "官職: 主簿 needs 4000 merit, leads 6000 soldiers and adds 1 politics (guide book p.6)");
    appoint.target = ids[1];
    engine_officer_edit low = feature_edit_of(api, ids[1]);
    low.merit = 100;
    api->set_officer(e, ids[1], &low);
    TEST_CHECK(counts, api->command_court(e, ENGINE_COMMAND_APPOINT, &appoint).refusal == SIM_REFUSAL_NO_MERIT, "refused: not sufficient merit");
    edit = feature_edit_of(api, ids[1]);
    edit.merit = 40000;
    api->set_officer(e, ids[1], &edit);
    TEST_CHECK(counts, api->command_court(e, ENGINE_COMMAND_APPOINT, &appoint).refusal == SIM_REFUSAL_POST_TAKEN, "refused: another officer holds the post");
    appoint.post = FEATURE_POST_CHANCELLOR;
    TEST_CHECK(counts, api->faction_rank(e, setup->force) >= 7 || api->command_court(e, ENGINE_COMMAND_APPOINT, &appoint).refusal == SIM_REFUSAL_NO_RANK,
               "refused: the rank of the ruler does not allow 丞相 (assumed rank rule, Q-027)");
}

internal u8
feature_clue_of(u32 chance)
{
    return chance < 34 ? (u8)SIM_CLUE_LOW : (chance >= 67 ? (u8)SIM_CLUE_HIGH : (u8)SIM_CLUE_MIDDLE);
}

/* An officer of another faction that the faction can recruit after the
   editor sets its loyalty to 0, or ENGINE_NO_OFFICER. */
internal u16
feature_recruit_target(engine_api *api, u8 other, engine_court_order *order)
{
    void *e = api->engine;
    for (u32 index = 0; index < api->officer_count(e); ++index)
    {
        engine_officer candidate = api->officer_at(e, index);
        if (candidate.force_id != other || candidate.status == OFFICER_STATUS_RULER || candidate.status > 3 || candidate.current_position >= 87) continue;
        engine_officer_edit edit = feature_edit_of(api, candidate.officer_id);
        edit.loyalty = 0;
        api->set_officer(e, candidate.officer_id, &edit);
        order->target = candidate.officer_id;
        if (feature_court_check(api, ENGINE_COMMAND_RECRUIT, order).refusal == SIM_REFUSAL_NONE) return candidate.officer_id;
    }
    order->target = ENGINE_NO_OFFICER;
    return ENGINE_NO_OFFICER;
}

/* The estimate of the adviser (court_advice) for 探索 and 登庸. */
internal void
feature_court_advice_tests(test_counts *counts, feature_city_setup *setup)
{
    engine_api *api = setup->api;
    void *e = api->engine;
    u16 ids[3];
    feature_take_officers(api, setup->city, ids, 1);
    engine_court_order order = feature_court_order(setup->city, ids[0], ENGINE_NO_OFFICER);
    engine_court_advice search = api->court_advice(e, ENGINE_COMMAND_SEARCH, &order);
    engine_check check = feature_court_check(api, ENGINE_COMMAND_SEARCH, &order);
    TEST_CHECK(counts, search.chance <= 100 && search.chance == check.quantity && search.clue == feature_clue_of(search.chance) &&
                       search.adviser == api->adviser(e, setup->force),
               "探索: court_advice gives the chance of the check and its band (advice.low_below 34, advice.high_from 67, assumed)");
    engine_court_advice none = api->court_advice(e, ENGINE_COMMAND_RECRUIT, &order);
    engine_court_advice reward = api->court_advice(e, ENGINE_COMMAND_REWARD, &order);
    TEST_CHECK(counts, none.clue == SIM_CLUE_NONE && none.chance == 0 && reward.clue == SIM_CLUE_NONE, "登庸 without a target and 褒賞 give no estimate");
    u8 other = feature_other_faction(api, setup->force, 0);
    u16 target = feature_recruit_target(api, other, &order);
    engine_court_advice recruit = api->court_advice(e, ENGINE_COMMAND_RECRUIT, &order);
    check = feature_court_check(api, ENGINE_COMMAND_RECRUIT, &order);
    TEST_CHECK(counts, target != ENGINE_NO_OFFICER && recruit.chance == check.quantity && recruit.clue == feature_clue_of(recruit.chance),
               "登庸: court_advice gives the chance of the recruiter for the target and its band");
}

internal void
feature_recruit_and_item_tests(test_counts *counts, feature_city_setup *setup)
{
    engine_api *api = setup->api;
    void *e = api->engine;
    u8 other_city = ENGINE_NO_CITY;
    u8 other = feature_other_faction(api, setup->force, &other_city);
    u16 recruiter[3];
    feature_take_officers(api, setup->city, recruiter, 1);
    engine_court_order order = feature_court_order(setup->city, recruiter[0], api->force_ruler(e, other));
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_RECRUIT, &order).refusal == SIM_REFUSAL_NOT_RECRUITABLE, "refused: a ruler cannot be recruited");
    u16 target = ENGINE_NO_OFFICER;
    for (u32 index = 0; index < api->officer_count(e) && target == ENGINE_NO_OFFICER; ++index)
    {
        engine_officer candidate = api->officer_at(e, index);
        if (candidate.force_id != other || candidate.status == OFFICER_STATUS_RULER || candidate.status > 3 || candidate.current_position >= 87 ||
            candidate.intelligence >= 85) continue;
        engine_officer_edit edit = feature_edit_of(api, candidate.officer_id);
        edit.loyalty = 100;
        api->set_officer(e, candidate.officer_id, &edit);
        order.target = candidate.officer_id;
        b32 loyal = feature_court_check(api, ENGINE_COMMAND_RECRUIT, &order).refusal == SIM_REFUSAL_NOT_RECRUITABLE;
        edit.loyalty = 0;
        api->set_officer(e, candidate.officer_id, &edit);
        if (loyal && feature_court_check(api, ENGINE_COMMAND_RECRUIT, &order).refusal == SIM_REFUSAL_NONE) target = candidate.officer_id;
    }
    order.target = target;
    engine_check check = feature_court_check(api, ENGINE_COMMAND_RECRUIT, &order);
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_court(e, ENGINE_COMMAND_RECRUIT, &order);
    b32 joined = feature_officer_force(api, target) == setup->force;
    TEST_CHECK(counts, target != ENGINE_NO_OFFICER && check.action_points == 20 && result.refusal == SIM_REFUSAL_NONE &&
                       (joined == (feature_count_events(api, SIM_EVENT_OFFICER_RECRUITED, mark) == 1)) &&
                       (check.quantity < 100 || joined),
               "登庸: 20 action points; loyalty 100 cannot be recruited; with loyalty 0 the chance is high and the officer joins");
    TEST_CHECK(counts, !joined || api->officer_detail(e, target).loyalty == 70, "a recruited officer has loyalty 70 (assumed)");

    /* 授與 */
    u16 giver[3];
    feature_take_officers(api, setup->city, giver, 2);
    u8 item_id = ENGINE_NO_ROW, foreign = ENGINE_NO_ROW;
    for (u32 index = 0; index < api->item_count(e); ++index)
    {
        engine_item item = api->item_at(e, index);
        if (item.id == ENGINE_NO_ROW) continue;
        if (item_id == ENGINE_NO_ROW) item_id = item.id;
        else if (foreign == ENGINE_NO_ROW) foreign = item.id;
    }
    api->set_item_owner(e, item_id, giver[0]);
    api->set_item_owner(e, foreign, api->force_ruler(e, other));
    engine_officer_edit edit = feature_edit_of(api, giver[1]);
    edit.loyalty = 50;
    api->set_officer(e, giver[1], &edit);
    engine_court_order give = feature_court_order(setup->city, ENGINE_NO_OFFICER, giver[1]);
    give.item = foreign;
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_GIVE_ITEM, &give).refusal == SIM_REFUSAL_NO_ITEM, "refused: the faction does not hold the item");
    give.item = item_id;
    u32 strength = api->item_at(e, item_id).strength;
    mark = api->ledger_count(e);
    engine_command_result given = api->command_court(e, ENGINE_COMMAND_GIVE_ITEM, &give);
    TEST_CHECK(counts, given.refusal == SIM_REFUSAL_NONE && api->item_at(e, item_id).owner == giver[1] &&
                       api->officer_detail(e, giver[1]).loyalty == Minimum(50u + strength, 100u) && feature_count_events(api, SIM_EVENT_ITEM_GIVEN, mark) == 1,
               "授與: the item goes to the officer and raises loyalty by its value (manual p.23)");
}

internal void
feature_match_release_tests(test_counts *counts, feature_city_setup *setup)
{
    engine_api *api = setup->api;
    void *e = api->engine;
    u16 a = feature_find_officer(api, setup->force, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    u16 b = feature_find_officer(api, setup->force, a, ENGINE_NO_OFFICER);
    for (u32 index = 0; index < 2; ++index)
    {
        u16 id = index ? b : a;
        engine_officer_edit edit = feature_edit_of(api, id);
        edit.merit  = 600;
        edit.female = false;
        api->set_officer(e, id, &edit);
    }
    engine_court_order match = feature_court_order(setup->city, a, b);
    match.kind = SIM_MATCH_MARRIAGE;
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_MATCHMAKE, &match).refusal == SIM_REFUSAL_NO_TECHNIQUE_POINTS ||
                       feature_court_check(api, ENGINE_COMMAND_MATCHMAKE, &match).refusal == SIM_REFUSAL_NOT_MATCHABLE,
               "refused: 仲介 costs 500 technique points; two men cannot marry");
    api->set_technique_points(e, setup->force, 600);
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_MATCHMAKE, &match).refusal == SIM_REFUSAL_NOT_MATCHABLE, "refused: two men cannot marry");
    match.kind = SIM_MATCH_SWORN;
    u32 mark = api->ledger_count(e);
    engine_command_result sworn = api->command_court(e, ENGINE_COMMAND_MATCHMAKE, &match);
    engine_officer oa = {}, ob = {};
    feature_officer_by_id(api, a, &oa);
    feature_officer_by_id(api, b, &ob);
    TEST_CHECK(counts, sworn.refusal == SIM_REFUSAL_NONE && oa.sworn_brother_group_head == ob.sworn_brother_group_head &&
                       oa.sworn_brother_group_head != ENGINE_NO_OFFICER && api->technique_points(e, setup->force) == 100 &&
                       feature_count_events(api, SIM_EVENT_SWORN, mark) == 1,
               "仲介: two officers with merit 500 become sworn brothers for 500 technique points");

    /* 流放 of a captive and of an own officer */
    u8 other = feature_other_faction(api, setup->force, 0);
    u16 prisoner = feature_find_officer(api, other, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    engine_officer_edit edit = feature_edit_of(api, prisoner);
    edit.captor = setup->force;
    api->set_officer(e, prisoner, &edit);
    TEST_CHECK(counts, api->officer_detail(e, prisoner).captive && feature_officer_force(api, prisoner) == setup->force,
               "the editor makes an officer a captive of the faction");
    engine_court_order release = feature_court_order(setup->city, ENGINE_NO_OFFICER, prisoner);
    u32 points = api->technique_points(e, setup->force);
    mark = api->ledger_count(e);
    TEST_CHECK(counts, api->command_court(e, ENGINE_COMMAND_RELEASE, &release).refusal == SIM_REFUSAL_NONE && feature_officer_force(api, prisoner) == other &&
                       !api->officer_detail(e, prisoner).captive && api->technique_points(e, setup->force) == points + 3 &&
                       feature_count_events(api, SIM_EVENT_CAPTIVE_RELEASED, mark) == 1,
               "流放 of a captive: it goes back to its faction, and the faction gains technique points (tech-tree.md: 2 to 5)");
    TEST_CHECK(counts, api->command_court(e, ENGINE_COMMAND_RELEASE, &release).refusal == SIM_REFUSAL_NOT_CAPTIVE, "refused: the officer is no captive");
    engine_court_order exile = feature_court_order(setup->city, ENGINE_NO_OFFICER, b);
    engine_officer exiled = {};
    TEST_CHECK(counts, feature_officer_by_id(api, b, &exiled), "the officer to exile is known");
    u32 best = Maximum(Maximum(Maximum((u32)exiled.leadership, (u32)exiled.war), Maximum((u32)exiled.intelligence, (u32)exiled.politics)), (u32)exiled.charm);
    u32 cost = 5 + 5 * best / 100;
    points = api->technique_points(e, setup->force);
    engine_command command = {};
    command.kind  = ENGINE_COMMAND_EXILE;
    command.court = exile;
    TEST_CHECK(counts, api->can_issue(e, &command).quantity == cost, "流放 of an own officer costs 5 technique points plus 5 scaled by the best ability (tech-tree.md)");
    TEST_CHECK(counts, api->command_court(e, ENGINE_COMMAND_EXILE, &exile).refusal == SIM_REFUSAL_NONE && feature_officer_by_id(api, b, &exiled) &&
                       exiled.force_id == ENGINE_NO_FACTION && exiled.status == OFFICER_STATUS_FREE_OR_NOT_APPEARED &&
                       api->technique_points(e, setup->force) == points - cost,
               "流放 of an own officer: it becomes a free officer, and the faction pays the points");
}

/* G2-063: the envoy goes on the road, and the result comes at its
   arrival. Ends the turns of the faction until the arrival (at most 12). */
internal b32
feature_wait_envoy(engine_api *api, u8 force, u32 mark)
{
    for (u32 turn = 0; turn < 12; ++turn)
    {
        if (feature_count_events(api, SIM_EVENT_ENVOY_ARRIVED, mark)) return true;
        if (!feature_next_own_turn(api, force)) return false;
    }
    return feature_count_events(api, SIM_EVENT_ENVOY_ARRIVED, mark) > 0;
}

internal void
feature_diplomacy_tests(test_counts *counts, feature_city_setup *setup)
{
    engine_api *api = setup->api;
    void *e = api->engine;
    u8 other_city = ENGINE_NO_CITY;
    u8 other = feature_other_faction(api, setup->force, &other_city);
    api->set_city_gold(e, setup->city, 60000);
    u16 envoys[3];
    feature_take_officers(api, setup->city, envoys, 3);
    u16 envoy = ENGINE_NO_OFFICER;
    for (u32 place = 0; place < 3 && envoy == ENGINE_NO_OFFICER; ++place)
    {
        engine_officer officer = {};
        if (feature_officer_by_id(api, envoys[place], &officer) && officer.skill_number + 1 != FEATURE_SKILL_DEBATER) envoy = envoys[place];
    }
    engine_court_order order = feature_court_order(setup->city, envoy, ENGINE_NO_OFFICER);
    order.force = other;
    order.kind  = SIM_DIPLOMACY_GOODWILL;
    u32 before = api->diplomacy(e, setup->force, other).friendliness;
    engine_check check = feature_court_check(api, ENGINE_COMMAND_DIPLOMACY, &order);
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_court(e, ENGINE_COMMAND_DIPLOMACY, &order);
    sim_event started = {};
    TEST_CHECK(counts, before == 50 && check.gold == 1000 && check.action_points == 30 && check.quantity == 100 && result.refusal == SIM_REFUSAL_NONE &&
                       api->diplomacy(e, setup->force, other).friendliness == before &&
                       feature_find_event(api, SIM_EVENT_MISSION_STARTED, mark, &started) && started.delta > 0,
               "親善: 1000 gold and 30 action points; the envoy goes on the road, and nothing changes at the order (G2-063)");
    TEST_CHECK(counts, feature_wait_envoy(api, setup->force, mark) &&
                       api->diplomacy(e, setup->force, other).friendliness > before && api->diplomacy(e, other, setup->force).friendliness > before,
               "親善 never fails: at the arrival of the envoy the friendliness of both sides rises");

    feature_next_own_turn(api, setup->force);
    api->set_actions(e, setup->force, 255);
    api->set_diplomacy(e, setup->force, other, 100, RELATION_NONE, 0);
    order.kind = SIM_DIPLOMACY_ALLIANCE;
    order.gold = 500;
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_DIPLOMACY, &order).refusal == SIM_REFUSAL_NO_AMOUNT, "refused: 同盟 needs 1000 gold or more");
    order.gold = 20000;
    check = feature_court_check(api, ENGINE_COMMAND_DIPLOMACY, &order);
    mark = api->ledger_count(e);
    result = api->command_court(e, ENGINE_COMMAND_DIPLOMACY, &order);
    TEST_CHECK(counts, check.quantity == 100 && result.refusal == SIM_REFUSAL_NONE && feature_wait_envoy(api, setup->force, mark) &&
                       api->diplomacy(e, setup->force, other).relation == RELATION_ALLIANCE,
               "同盟: high friendliness and much gold give an alliance at the arrival of the envoy (assumed chance)");
    api->set_actions(e, setup->force, 255);
    feature_take_officers(api, setup->city, envoys, 1);
    order.officer = envoys[0];
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_DIPLOMACY, &order).refusal == SIM_REFUSAL_RELATION, "refused: the factions are allies");

    /* 請求援軍 */
    feature_next_own_turn(api, setup->force);
    api->set_actions(e, setup->force, 255);
    feature_take_officers(api, setup->city, envoys, 1);
    engine_court_order aid = feature_court_order(setup->city, envoys[0], ENGINE_NO_OFFICER);
    aid.force = other;
    aid.kind = SIM_DIPLOMACY_AID;
    aid.target_city = setup->city;
    aid.gold = 10000;
    mark = api->ledger_count(e);
    result = api->command_court(e, ENGINE_COMMAND_DIPLOMACY, &aid);
    sim_event arrived = {};
    b32 found = result.refusal == SIM_REFUSAL_NONE && feature_wait_envoy(api, setup->force, mark) &&
                feature_find_event(api, SIM_EVENT_AID_ARRIVED, mark, &arrived);
    TEST_CHECK(counts, found && api->unit_at(e, arrived.detail).force_id == other,
               "請求援軍: at the arrival of the envoy the ally sends a unit next to the city (aid.turns = 18)");

    /* 摒棄 */
    feature_next_own_turn(api, setup->force);
    api->set_actions(e, setup->force, 255);
    api->set_technique_points(e, setup->force, 1000);
    feature_take_officers(api, setup->city, envoys, 1);
    engine_court_order brk = feature_court_order(setup->city, envoys[0], ENGINE_NO_OFFICER);
    brk.force = other;
    brk.kind  = SIM_DIPLOMACY_BREAK;
    mark = api->ledger_count(e);
    result = api->command_court(e, ENGINE_COMMAND_DIPLOMACY, &brk);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_wait_envoy(api, setup->force, mark) &&
                       api->diplomacy(e, setup->force, other).relation == RELATION_NONE && api->technique_points(e, setup->force) == 1050,
               "摒棄: at the arrival of the envoy the alliance ends, and the side that proposes it gains 50 technique points (tech-tree.md)");
    api->set_actions(e, setup->force, 255);
    engine_court_order ceasefire = brk;
    ceasefire.kind   = SIM_DIPLOMACY_CEASEFIRE;
    ceasefire.months = 30;
    feature_take_officers(api, setup->city, envoys, 1);
    ceasefire.officer = envoys[0];
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_DIPLOMACY, &ceasefire).refusal == SIM_REFUSAL_NO_AMOUNT, "refused: a ceasefire lasts 2 to 24 months");
    ceasefire.kind = SIM_DIPLOMACY_GOODWILL;
    ceasefire.force = setup->force;
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_DIPLOMACY, &ceasefire).refusal == SIM_REFUSAL_NO_FACTION, "refused: diplomacy with the own faction");

    /* 交換俘虜 */
    u16 prisoner = feature_find_officer(api, setup->force, envoys[0], ENGINE_NO_OFFICER);   /* not the envoy */
    engine_officer_edit edit = feature_edit_of(api, prisoner);
    edit.captor = other;
    api->set_officer(e, prisoner, &edit);
    engine_court_order exchange = feature_court_order(setup->city, envoys[0], prisoner);
    exchange.force = other;
    exchange.kind  = SIM_DIPLOMACY_EXCHANGE;
    exchange.gold  = 20000;
    engine_officer envoy_officer = {};
    feature_officer_by_id(api, envoys[0], &envoy_officer);
    if (envoy_officer.skill_number + 1 != FEATURE_SKILL_DEBATER)
    {
        mark = api->ledger_count(e);
        result = api->command_court(e, ENGINE_COMMAND_DIPLOMACY, &exchange);
        b32 waited = result.refusal == SIM_REFUSAL_NONE && feature_wait_envoy(api, setup->force, mark);
        /* On the road the other faction can recruit the captive (its AI): then the envoy finds no captive. */
        b32 back  = feature_officer_force(api, prisoner) == setup->force && !api->officer_detail(e, prisoner).captive;
        b32 taken = feature_officer_force(api, prisoner) == other && !api->officer_detail(e, prisoner).captive;
        TEST_CHECK(counts, waited && (back || taken),
                   "交換俘虜: for much gold the other faction gives the captive back at the arrival of the envoy, unless it recruited the captive first");
    }
}

internal void
feature_faction_plot_tests(test_counts *counts, feature_city_setup *setup)
{
    engine_api *api = setup->api;
    void *e = api->engine;
    api->set_city_gold(e, setup->city, 60000);
    u16 actors[3];
    feature_take_officers(api, setup->city, actors, 2);
    engine_court_order plot = feature_court_order(setup->city, actors[0], ENGINE_NO_OFFICER);
    plot.kind = SIM_PLOT_RUMOUR;
    plot.target_city = setup->city;
    TEST_CHECK(counts, feature_court_check(api, ENGINE_COMMAND_FACTION_PLOT, &plot).refusal == SIM_REFUSAL_NO_CITY, "refused: 流言 on an own city");
    /* 驅虎吞狼 on a city whose governor is not its ruler: low loyalty, high ambition. */
    u8 target_city = ENGINE_NO_CITY;
    for (u32 index = 0; index < api->city_count(e) && target_city == ENGINE_NO_CITY; ++index)
    {
        engine_city city = api->city_at(e, index);
        if (city.owner_force_id == setup->force || city.owner_force_id == ENGINE_NO_FACTION) continue;
        u16 governor = api->city_slots(e, city.city_id).governor_id;
        if (governor == ENGINE_NO_OFFICER || governor == api->force_ruler(e, city.owner_force_id)) continue;
        engine_officer_edit edit = feature_edit_of(api, governor);
        edit.loyalty  = 0;
        edit.ambition = 4;
        api->set_officer(e, governor, &edit);
        target_city = city.city_id;
    }
    plot.kind = SIM_PLOT_DRIVE_TIGER;
    plot.target_city = target_city;
    u8 old_owner = api->city_at(e, 0).owner_force_id;
    for (u32 index = 0; index < api->city_count(e); ++index) if (api->city_at(e, index).city_id == target_city) old_owner = api->city_at(e, index).owner_force_id;
    engine_check check = feature_court_check(api, ENGINE_COMMAND_FACTION_PLOT, &plot);
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_court(e, ENGINE_COMMAND_FACTION_PLOT, &plot);
    /* G2-065: the plot goes on the road; its result comes at the turn start of the arrival (travel.cpp). */
    u32 turns = 0;
    while (turns < 12 && feature_count_events(api, SIM_EVENT_FACTION_PLOT, mark) + feature_count_events(api, SIM_EVENT_FACTION_PLOT_FAILED, mark) == 0 &&
           feature_next_own_turn(api, setup->force)) turns++;
    sim_event founded = {};
    b32 success = feature_find_event(api, SIM_EVENT_FACTION_FOUNDED, mark, &founded);
    u8 new_owner = ENGINE_NO_FACTION;
    for (u32 index = 0; index < api->city_count(e); ++index) if (api->city_at(e, index).city_id == target_city) new_owner = api->city_at(e, index).owner_force_id;
    TEST_CHECK(counts, target_city != ENGINE_NO_CITY && check.gold == 200 && check.action_points == 30 && result.refusal == SIM_REFUSAL_NONE &&
                       feature_count_events(api, SIM_EVENT_FACTION_PLOT, mark) + feature_count_events(api, SIM_EVENT_FACTION_PLOT_FAILED, mark) == 1 &&
                       (check.quantity < 100 || success),
               "驅虎吞狼: 200 gold, 30 action points; an ambitious governor with low loyalty founds a faction at the arrival of the plot");
    TEST_CHECK(counts, !success || (new_owner == founded.faction && new_owner != old_owner && api->force_ruler(e, new_owner) != ENGINE_NO_OFFICER),
               "the governor rules the new faction and its city");
}

/* G2-039: a search gives one result to the player: an officer found,
   gold found, or the line of a failure. */
internal void
feature_search_result_tests(test_counts *counts, feature_city_setup *setup)
{
    engine_api *api = setup->api;
    void *e = api->engine;
    api->set_actions(e, setup->force, 255);
    u16 ids[3];
    TEST_CHECK(counts, feature_take_officers(api, setup->city, ids, 1) >= 1, "a free officer searches");
    engine_court_order order = feature_court_order(setup->city, ids[0], ENGINE_NO_OFFICER);
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_court(e, ENGINE_COMMAND_SEARCH, &order);
    u32 found  = feature_count_events(api, SIM_EVENT_OFFICER_FOUND, mark);
    u32 gold   = feature_count_events(api, SIM_EVENT_GOLD_FOUND, mark);
    u32 failed = feature_count_events(api, SIM_EVENT_SEARCH_FAILED, mark);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && found + gold + failed == 1,
               "探索 gives one result: an officer, gold, or the line of a failure (G2-039)");
}

internal void
feature_court_tests_main(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    feature_court_table_tests(counts, &setup);
    feature_reward_and_post_tests(counts, &setup);
    feature_search_result_tests(counts, &setup);
    feature_court_advice_tests(counts, &setup);
    feature_recruit_and_item_tests(counts, &setup);
    feature_match_release_tests(counts, &setup);
    engine_fixture_destroy(&setup.fixture);

    feature_city_setup_init(&setup);
    feature_diplomacy_tests(counts, &setup);
    engine_fixture_destroy(&setup.fixture);

    feature_city_setup_init(&setup);
    feature_faction_plot_tests(counts, &setup);
    engine_fixture_destroy(&setup.fixture);
}

/* ---- duel, debate and the month ----------------------------------------------------- */

internal void
feature_duel_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u32 a = feature_sortie_unit(api, setup.city, 1);
    u32 b = feature_sortie_unit(api, setup.city, 1);
    u8 enemy = feature_other_faction(api, setup.force, 0);
    hex_coord at_a = feature_unit_hex(api, a);
    hex_coord spot = at_a;
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        hex_coord hex = hex_neighbor(at_a, d);
        if (api->unit_at_hex(e, hex.q, hex.r) == ENGINE_NO_UNIT && api->set_unit_place(e, b, enemy, hex.q, hex.r)) { spot = hex; break; }
    }
    engine_unit unit_b = api->unit_at(e, b);
    engine_officer_edit edit = feature_edit_of(api, unit_b.officers[0]);
    edit.personality = PERSONALITY_RECKLESS;
    api->set_officer(e, unit_b.officers[0], &edit);
    engine_command duel = {};
    duel.kind = ENGINE_COMMAND_DUEL;
    duel.unit = a;
    duel.q = spot.q; duel.r = spot.r;
    engine_check check = api->can_issue(e, &duel);
    TEST_CHECK(counts, !hex_equal(spot, at_a) && check.refusal == SIM_REFUSAL_NONE && check.quantity >= 45 && check.assumed,
               "單挑: can_issue gives the chance that the target accepts (a reckless officer accepts often)");
    b32 started = false;
    for (u32 attempt = 0; attempt < 8 && !started; ++attempt)
    {
        api->command_duel(e, a, spot.q, spot.r);
        started = api->duel_view(e).active;
        if (!started) feature_next_own_turn(api, setup.force);
    }
    engine_duel_view view = api->duel_view(e);
    TEST_CHECK(counts, started && view.round == 0 && view.side[0].officers[0] == api->unit_at(e, a).officers[0] && view.side[0].stamina[0] > 0,
               "the target accepts and the duel opens with the two commanders");
    /* The play report of 2026-10-23: a duel of two computer officers runs to its end at once, with no screen. */
    {
        feature_city_setup other;
        feature_march_setup(&other);
        engine_api *api_2 = other.api;
        void *e_2 = api_2->engine;
        u32 c = feature_sortie_unit(api_2, other.city, 1);
        u32 d = feature_sortie_unit(api_2, other.city, 1);
        u8 enemy_2 = feature_other_faction(api_2, other.force, 0);
        hex_coord at_c = feature_unit_hex(api_2, c);
        hex_coord spot_2 = at_c;
        for (u32 dir = 0; dir < HEX_DIRECTION_COUNT; ++dir)
        {
            hex_coord hex = hex_neighbor(at_c, dir);
            if (api_2->unit_at_hex(e_2, hex.q, hex.r) == ENGINE_NO_UNIT && api_2->set_unit_place(e_2, d, enemy_2, hex.q, hex.r)) { spot_2 = hex; break; }
        }
        engine_officer_edit edit_2 = feature_edit_of(api_2, api_2->unit_at(e_2, d).officers[0]);
        edit_2.personality = PERSONALITY_RECKLESS;
        api_2->set_officer(e_2, api_2->unit_at(e_2, d).officers[0], &edit_2);
        u32 mark_2 = api_2->ledger_count(e_2);
        b32 started_2 = false;
        for (u32 attempt = 0; attempt < 8 && !started_2; ++attempt)
        {
            api_2->set_faction_human(e_2, other.force, false);   /* two computer factions at the start of the duel */
            api_2->command_duel(e_2, c, spot_2.q, spot_2.r);
            started_2 = feature_count_events(api_2, SIM_EVENT_DUEL_STARTED, mark_2) > 0;
            api_2->set_faction_human(e_2, other.force, true);
            if (!started_2) feature_next_own_turn(api_2, other.force);   /* a refused challenge took the action of the unit */
        }
        TEST_CHECK(counts, started_2 && !api_2->duel_view(e_2).active && feature_count_events(api_2, SIM_EVENT_DUEL_ENDED, mark_2) == 1,
                   "a duel of two computer officers ends at once: the player sees only the result");
        engine_fixture_destroy(&other.fixture);
    }
    TEST_CHECK(counts, api->command_attack(e, a, spot.q, spot.r).refusal == SIM_REFUSAL_BUSY, "refused: other commands wait while a duel runs");
    TEST_CHECK(counts, api->command_duel_action(e, SIM_DUEL_SPECIAL, DUEL_PEERLESS).refusal == SIM_REFUSAL_NO_SPIRIT, "refused: 無雙 needs a spirit of 300");
    u32 mark = api->ledger_count(e);
    TEST_CHECK(counts, api->command_duel_action(e, SIM_DUEL_POLICY, DUEL_ATTACK).refusal == SIM_REFUSAL_NONE && api->duel_view(e).side[0].policy == DUEL_ATTACK &&
                       api->command_duel_action(e, SIM_DUEL_ROUND, 0).refusal == SIM_REFUSAL_NONE && api->duel_view(e).round == 1 &&
                       feature_count_events(api, SIM_EVENT_DUEL_STRIKE, mark) >= 1,
               "a policy, then a round: the fighters strike and the stamina falls (assumed damage)");
    mark = api->ledger_count(e);
    api->command_duel_action(e, SIM_DUEL_AUTO, 0);
    view = api->duel_view(e);
    TEST_CHECK(counts, !view.active && view.result != DUEL_RESULT_NONE && feature_count_events(api, SIM_EVENT_DUEL_ENDED, mark) == 1,
               "自動: the duel plays to its end and the ledger has the result");
    TEST_CHECK(counts, view.result == DUEL_RESULT_DRAW || feature_count_events(api, SIM_EVENT_MERIT_GAINED, mark) >= 1, "the winner gains merit");
    /* G3-002 (observed 2026-10-23): the unit of the loser loses 30 percent of its soldiers (1000 to 700), and
       the loser becomes a captive of the force of the winner, unless 強運 or 護衛 saves him. */
    if (view.winner < 2 && view.result != DUEL_RESULT_RETREAT)
    {
        engine_duel_side losing = view.side[1 - view.winner];
        u16 loser = losing.officers[losing.fighter];
        sim_event hit = {};
        b32 lost_soldiers = feature_find_event(api, SIM_EVENT_UNIT_DAMAGED, mark, &hit) && hit.delta == -300;
        engine_officer loser_officer = api->officer_by_id(e, loser);
        b32 saved = loser_officer.skill_number != 255 && (loser_officer.skill_number == 32 || loser_officer.skill_number == 52);   /* 強運, 護衛 */
        TEST_CHECK(counts, lost_soldiers && (saved || api->officer_detail(e, loser).captive),
                   "the unit of the loser loses 30 percent of its soldiers, and the loser is a captive of the winner (G3-002)");
    }
    engine_fixture_destroy(&setup.fixture);
}

/* G3-019 (observed 2026-10-23): at a month start the own officers keep their
   loyalty, and each captive loses 2 or 3. */
internal void
feature_month_loyalty_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 other = feature_other_faction(api, setup.force, 0);
    u16 prisoner = feature_find_officer(api, other, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    engine_officer_edit edit = feature_edit_of(api, prisoner);
    edit.captor = setup.force;
    api->set_officer(e, prisoner, &edit);
    u16 own = feature_find_officer(api, setup.force, api->force_ruler(e, setup.force), ENGINE_NO_OFFICER);
    u8 own_before = api->officer_detail(e, own).loyalty, captive_before = api->officer_detail(e, prisoner).loyalty;
    u8 month = api->date(e).month;
    u32 turns = 0;
    for (; turns < 6 && api->date(e).month == month; ++turns) feature_next_own_turn(api, setup.force);
    engine_officer_detail own_after = api->officer_detail(e, own), captive_after = api->officer_detail(e, prisoner);
    s32 captive_loss = (s32)captive_before - (s32)captive_after.loyalty;
    TEST_CHECK(counts, api->date(e).month != month && own_after.loyalty == own_before,
               "an own officer keeps its loyalty at the month start (G3-019, observed)");
    TEST_CHECK(counts, !captive_after.captive || captive_loss == 2 || captive_loss == 3,
               "a captive loses 2 or 3 loyalty at the month start (G3-019, observed)");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_debate_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u16 a = ENGINE_NO_OFFICER, b = ENGINE_NO_OFFICER;
    for (u32 i = 0; i < api->officer_count(e) && b == ENGINE_NO_OFFICER; ++i)
    {
        engine_officer x = api->officer_at(e, i);
        if (x.force_id != setup.force || x.status > 3) continue;
        if (a == ENGINE_NO_OFFICER) { a = x.officer_id; continue; }
        engine_officer first = {};
        feature_officer_by_id(api, a, &first);
        s32 gap = (s32)first.intelligence - (s32)x.intelligence;
        if (gap > -20 && gap < 20) b = x.officer_id;
    }
    engine_command_result started = api->start_debate(e, a, b);
    engine_debate_view view = api->debate_view(e);
    engine_officer oa = {};
    feature_officer_by_id(api, a, &oa);
    TEST_CHECK(counts, started.refusal == SIM_REFUSAL_NONE && view.active && !view.finished && view.side[0].mind == 100 && view.side[0].rocks == 5 &&
                       view.side[0].hand_count == sim_debate_hand_size(oa.intelligence),
               "a debate opens: a full mind gauge with 5 rocks and a hand by intellect (guide book p.30)");
    TEST_CHECK(counts, api->start_debate(e, a, b).refusal == SIM_REFUSAL_BUSY, "refused: one debate at a time");
    u32 mark = api->ledger_count(e);
    engine_command_result spoke = view.turn == 0 ? api->command_debate_action(e, SIM_DEBATE_PLAY, 0) : api->command_debate_action(e, SIM_DEBATE_ROUND, 0);
    view = api->debate_view(e);
    TEST_CHECK(counts, spoke.refusal == SIM_REFUSAL_NONE && (view.round == 1 || view.finished) &&
                       (view.last[0].type != CARD_TYPE_COUNT || view.last[1].type != CARD_TYPE_COUNT || view.finished),
               "one exchange: a side plays a card, the other answers or takes the hit");
    api->command_debate_action(e, SIM_DEBATE_AUTO, 0);
    view = api->debate_view(e);
    TEST_CHECK(counts, view.active && view.finished && (view.side[0].mind <= 0 || view.side[1].mind <= 0 || view.round >= 1) &&
                       feature_count_events(api, SIM_EVENT_DEBATE_ENDED, mark) == 1,
               "自動: the debate plays until a mind gauge is empty");
    TEST_CHECK(counts, api->command_debate_action(e, SIM_DEBATE_FINISH, 0).refusal == SIM_REFUSAL_NONE && !api->debate_view(e).active,
               "the winner chooses 施予同情 and the debate closes");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_month_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u16 ruler = api->force_ruler(e, setup.force);
    u16 officer = ENGINE_NO_OFFICER, calm = ENGINE_NO_OFFICER;
    engine_officer ruler_officer = {};
    feature_officer_by_id(api, ruler, &ruler_officer);
    for (u32 i = 0; i < api->officer_count(e) && calm == ENGINE_NO_OFFICER; ++i)
    {
        engine_officer x = api->officer_at(e, i);
        if (x.force_id != setup.force || x.status == OFFICER_STATUS_RULER || x.status > 3) continue;
        if (x.sworn_brother_group_head != ENGINE_NO_OFFICER && x.sworn_brother_group_head == ruler_officer.sworn_brother_group_head) continue;
        if (x.spouse == ruler) continue;
        engine_officer_detail detail = api->officer_detail(e, x.officer_id);
        b32 liked = false;
        engine_officer_detail ruler_detail = api->officer_detail(e, ruler);
        for (u32 slot = 0; slot < OFFICER_RELATION_SLOTS; ++slot) liked |= ruler_detail.liked[slot] == x.officer_id;
        if (liked || detail.loyalty != 100) continue;   /* G2-035: the save gives 100 to most officers */
        if (officer == ENGINE_NO_OFFICER) officer = x.officer_id;
        else calm = x.officer_id;
    }
    engine_officer_edit edit = feature_edit_of(api, officer);
    edit.ambition = 4;
    api->set_officer(e, officer, &edit);
    edit = feature_edit_of(api, calm);
    edit.ambition = 0;
    edit.justice  = 4;
    api->set_officer(e, calm, &edit);
    u32 mark = api->ledger_count(e);
    for (u32 step = 0; step < 12 && feature_count_events(api, SIM_EVENT_MONTH_STARTED, mark) == 0; ++step) feature_next_own_turn(api, setup.force);
    u32 lost = 100 - api->officer_detail(e, officer).loyalty, kept = 100 - api->officer_detail(e, calm).loyalty;
    TEST_CHECK(counts, calm != ENGINE_NO_OFFICER && feature_count_events(api, SIM_EVENT_MONTH_STARTED, mark) >= 1 && lost >= 1 && lost > kept,
               "each month officers lose loyalty; an ambitious officer loses more than a just one (assumed amounts, Q-028)");
    engine_fixture_destroy(&setup.fixture);
}

/* G2-032: a captive escapes to its force at a month start, by the chance
   of captive.escape_percent (assumed, 10). Within 72 months the escape
   is near certain. The escape is an event with a report, so the turn
   start shows it with a message. */
internal void
feature_captive_escape_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 other = feature_other_faction(api, setup.force, 0);
    u16 prisoner = feature_find_officer(api, other, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    engine_officer_edit edit = feature_edit_of(api, prisoner);
    edit.captor = setup.force;
    api->set_officer(e, prisoner, &edit);
    TEST_CHECK(counts, prisoner != ENGINE_NO_OFFICER && api->officer_detail(e, prisoner).captive, "a captive of the player waits in its city");
    u32 mark = api->ledger_count(e);
    u32 turns = 0;
    for (; turns < 72 * 3 && api->officer_detail(e, prisoner).captive; ++turns) feature_next_own_turn(api, setup.force);
    b32 escaped = false;
    for (u32 index = mark; index < api->ledger_count(e); ++index)
    {
        sim_event event = api->ledger_at(e, index);
        if (event.kind == SIM_EVENT_CAPTIVE_ESCAPED && api->event_report(e, index).subject.id == prisoner) escaped = api->event_report(e, index).report;
    }
    char name[200];
    snprintf(name, sizeof(name), "the captive escapes to its force at a month start within 72 months (after %u turns), with a report", turns);
    TEST_CHECK(counts, escaped && !api->officer_detail(e, prisoner).captive && feature_officer_force(api, prisoner) == other, name);
    engine_fixture_destroy(&setup.fixture);
}

/* 俘虜處置 (ui/city-commands.md, "Prisoner dialog"): the four choices of
   the player for a captive through command_captive. 俘虜 keeps the
   captive, 釋放 sends it back, 處決 kills it, 登庸 recruits it with the
   chance of the ruler (assumed). The play report of 2026-10-23 asked for
   the dialog. */
internal void
feature_captive_choice_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 other = feature_other_faction(api, setup.force, 0);
    u16 first  = feature_find_officer(api, other, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    u16 second = feature_find_officer(api, other, first, ENGINE_NO_OFFICER);
    u16 third  = feature_find_officer(api, other, first, second);
    TEST_CHECK(counts, api->command_captive(e, first, ENGINE_CAPTIVE_RELEASE).refusal == SIM_REFUSAL_NOT_CAPTIVE,
               "refused: the choice for an officer that is no captive of the player");
    u16 prisoners[3] = { first, second, third };
    for (u32 index = 0; index < 3; ++index)
    {
        engine_officer_edit edit = feature_edit_of(api, prisoners[index]);
        edit.captor = setup.force;
        if (index == 2) edit.loyalty = 0;   /* the chance of the recruit is then 100 */
        api->set_officer(e, prisoners[index], &edit);
    }
    u32 mark = api->ledger_count(e);
    TEST_CHECK(counts, api->command_captive(e, first, ENGINE_CAPTIVE_KEEP).refusal == SIM_REFUSAL_NONE && api->officer_detail(e, first).captive &&
                       feature_count_events(api, SIM_EVENT_CAPTIVE_KEPT, mark) == 1,
               "the choice to keep leaves the captive in the prison, with a log line");
    mark = api->ledger_count(e);
    TEST_CHECK(counts, api->command_captive(e, first, ENGINE_CAPTIVE_RELEASE).refusal == SIM_REFUSAL_NONE && !api->officer_detail(e, first).captive &&
                       feature_officer_force(api, first) == other && feature_count_events(api, SIM_EVENT_CAPTIVE_RELEASED, mark) == 1,
               "the choice to release sends the captive back to its faction");
    mark = api->ledger_count(e);
    TEST_CHECK(counts, api->command_captive(e, second, ENGINE_CAPTIVE_EXECUTE).refusal == SIM_REFUSAL_NONE &&
                       api->officer_by_id(e, second).status == 8 && feature_count_events(api, SIM_EVENT_OFFICER_EXECUTED, mark) == 1 &&
                       feature_count_events(api, SIM_EVENT_OFFICER_DIED, mark) == 1,
               "the choice to execute kills the captive (assumed: no other effect)");
    engine_command probe = {};
    probe.kind = ENGINE_COMMAND_CAPTIVE;
    probe.court.target = third;
    probe.court.kind   = ENGINE_CAPTIVE_RECRUIT;
    engine_check chance = api->can_issue(e, &probe);
    mark = api->ledger_count(e);
    TEST_CHECK(counts, chance.refusal == SIM_REFUSAL_NONE && chance.quantity == 100 &&
                       api->command_captive(e, third, ENGINE_CAPTIVE_RECRUIT).refusal == SIM_REFUSAL_NONE &&
                       feature_officer_force(api, third) == setup.force && !api->officer_detail(e, third).captive &&
                       feature_count_events(api, SIM_EVENT_OFFICER_RECRUITED, mark) == 1,
               "the choice to recruit a captive with loyalty 0 joins it to the player (the ruler recruits, assumed)");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_court_tests(test_counts *counts)
{
    feature_court_tests_main(counts);
    feature_duel_tests(counts);
    feature_debate_tests(counts);
    feature_month_tests(counts);
    feature_captive_escape_tests(counts);
    feature_captive_choice_tests(counts);
    feature_month_loyalty_tests(counts);
}
