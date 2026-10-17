/* Unit tests of code/sim/economy.cpp and code/sim/city_commands.cpp: the
   production arithmetic, the price rule of the merchant and the quantity
   rules of the city commands (cities-and-facilities.md, city-commands.md,
   weapons-and-production.md, skills.md). The rules are the numbers of
   data/placeholder/rules.tsv. Where a rule is assumed, the check says so. */

internal sim_rules
unit_economy_rules(void)
{
    sim_rules rules = {};
    rules.order_max                    = 100;
    rules.order_income_floor           = 50;    /* assumed */
    rules.patrol_leadership_percent    = 10;    /* assumed */
    rules.patrol_enemy_percent         = 50;    /* assumed */
    rules.conscript_charm_soldiers     = 10;    /* assumed */
    rules.conscript_enemy_percent      = 50;    /* assumed */
    rules.train_force_percent          = 10;    /* assumed */
    rules.train_drill_percent          = 150;   /* assumed */
    rules.merchant_grain_per_gold      = 6;     /* assumed from the 市價 example of ui/city-commands.md */
    rules.merchant_politics_divisor    = 10;    /* assumed */
    rules.produce_intellect_quantity   = 10;    /* assumed */
    rules.produce_full_intellect       = 100;   /* assumed */
    rules.produce_specialty_percent    = 80;    /* assumed */
    rules.produce_skill_percent        = 150;   /* assumed */
    rules.produce_skill_period_percent = 50;    /* documented: half the time */
    rules.level2_percent               = 120;   /* documented */
    rules.level3_percent               = 150;   /* documented */
    rules.adjacency_percent            = 150;   /* documented */
    rules.skill_income_percent         = 150;   /* documented */
    rules.city_gold_income             = 100;
    rules.city_grain_income            = 500;
    rules.officer_salary               = 10;
    rules.grain_soldiers               = 100;
    rules.skill_rich  = 87;
    rules.skill_rice  = 88;
    rules.skill_tax   = 89;
    rules.skill_levy  = 90;
    return rules;
}

internal void
unit_economy_arithmetic_tests(test_counts *counts)
{
    sim_rules rules = unit_economy_rules();
    TEST_CHECK(counts, sim_percent_of(50, 150) == 75 && sim_percent_of(4000000000u, 50) == 2000000000u,
               "a percent of an amount uses 64-bit arithmetic");

    /* Merge levels (manual p.28): level 2 gives 1.2 times, level 3 gives 1.5 times. */
    TEST_CHECK(counts, sim_level_percent(1, &rules) == 100 && sim_level_percent(2, &rules) == 120 &&
                       sim_level_percent(3, &rules) == 150, "the level of a facility gives 100, 120 or 150 percent");
    TEST_CHECK(counts, sim_level_percent(0, &rules) == 100 && sim_level_percent(9, &rules) == 150,
               "a level out of the range counts as the nearest level");
    TEST_CHECK(counts, sim_facility_output(50, 1, false, &rules) == 50, "a level-1 market gives its value");
    TEST_CHECK(counts, sim_facility_output(50, 2, false, &rules) == 60, "a level-2 market gives 1.2 times");
    TEST_CHECK(counts, sim_facility_output(50, 3, false, &rules) == 75, "a level-3 market gives 1.5 times");
    TEST_CHECK(counts, sim_facility_output(50, 1, true, &rules) == 75, "a market next to a mint gives 1.5 times");
    TEST_CHECK(counts, sim_facility_output(50, 3, true, &rules) == 112,
               "a level-3 market next to a mint gives more again: 1.5 times 1.5, rounded down");

    /* Public order: high order raises the income (cities-and-facilities.md). The line is assumed. */
    TEST_CHECK(counts, sim_order_percent(100, &rules) == 100 && sim_order_percent(0, &rules) == 50 &&
                       sim_order_percent(60, &rules) == 80, "the income percent goes from 50 at order 0 to 100 at order 100 (assumed)");
    TEST_CHECK(counts, sim_order_percent(250, &rules) == 100, "an order above the maximum counts as the maximum");
    sim_rules no_order = {};
    TEST_CHECK(counts, sim_order_percent(30, &no_order) == 100, "without the order rules the income is not changed");
}

internal void
unit_city_command_rule_tests(test_counts *counts)
{
    sim_rules rules = unit_economy_rules();
    /* 巡察: the sum of leadership raises public order (assumed amount). */
    TEST_CHECK(counts, sim_patrol_rise(240, false, &rules) == 24, "patrol: a leadership sum of 240 gives 24 order (assumed)");
    TEST_CHECK(counts, sim_patrol_rise(240, true, &rules) == 12, "patrol: an enemy within 2 hexes gives a smaller rise (assumed half)");
    TEST_CHECK(counts, sim_patrol_rise(5, false, &rules) == 1, "patrol: one officer always gives at least 1 order");

    /* 徵兵: the sum of charm raises the number of soldiers (assumed amount). */
    TEST_CHECK(counts, sim_conscript_soldiers(240, 1, false, &rules) == 2400, "conscript: a charm sum of 240 gives 2400 soldiers (assumed)");
    TEST_CHECK(counts, sim_conscript_soldiers(240, 2, false, &rules) == 2880 && sim_conscript_soldiers(240, 3, false, &rules) == 3600,
               "conscript: a level-2 or level-3 barracks gives 1.2 or 1.5 times");
    TEST_CHECK(counts, sim_conscript_soldiers(240, 1, true, &rules) == 1200, "conscript: an enemy within 2 hexes gives fewer soldiers (assumed half)");

    /* 訓練: the sum of force raises morale (assumed amount). */
    TEST_CHECK(counts, sim_train_rise(240, false, &rules) == 24, "train: a force sum of 240 gives 24 morale (assumed)");
    TEST_CHECK(counts, sim_train_rise(240, true, &rules) == 36, "train: a drill ground gives a larger effect (assumed 1.5 times)");
    TEST_CHECK(counts, sim_train_rise(3, false, &rules) == 1, "train: one officer always gives at least 1 morale");

    /* 商人: the price rule is assumed. Politics gives a better price. */
    TEST_CHECK(counts, sim_merchant_percent(80, &rules) == 108 && sim_merchant_percent(0, &rules) == 100,
               "merchant: politics 80 gives 8 percent more (assumed)");
    TEST_CHECK(counts, sim_merchant_grain_for_gold(1000, 80, &rules) == 6480, "merchant: 1000 gold buys 6000 grain plus 8 percent (assumed price)");
    TEST_CHECK(counts, sim_merchant_gold_for_grain(6000, 80, &rules) == 1080, "merchant: 6000 grain sells for 1000 gold plus 8 percent (assumed price)");
    TEST_CHECK(counts, sim_merchant_gold_for_grain(5, 0, &rules) == 0, "merchant: less grain than one gold gives no gold");
    b32 no_profit = true;
    for (u32 politics = 0; politics <= 100; politics += 10)
    {
        u32 grain = sim_merchant_grain_for_gold(1000, politics, &rules);
        u32 back  = sim_merchant_gold_for_grain(grain, politics, &rules);
        if (back > sim_percent_of(sim_percent_of(1000, sim_merchant_percent(politics, &rules)), sim_merchant_percent(politics, &rules))) no_profit = false;
    }
    TEST_CHECK(counts, no_profit, "merchant: a buy and a sale give back no more than the price bonus two times");

    /* 生產兵裝: the sum of intellect sets the quantity, or the period of siege weapons and ships. */
    TEST_CHECK(counts, sim_produce_quantity(240, 1, false, &rules) == 2400, "produce: an intellect sum of 240 gives 2400 weapons (assumed)");
    TEST_CHECK(counts, sim_produce_quantity(240, 3, true, &rules) == 5400,
               "produce: a level-3 smithy and the skill 能吏 give 1.5 times 1.5 (skill amount assumed)");
    TEST_CHECK(counts, sim_produce_days(0, 30, 90, false, &rules) == 90 && sim_produce_days(100, 30, 90, false, &rules) == 30 &&
                       sim_produce_days(50, 30, 90, false, &rules) == 60,
               "produce: a siege weapon takes 90 days at intellect 0 and 30 days at full intellect (assumed line)");
    TEST_CHECK(counts, sim_produce_days(50, 30, 90, true, &rules) == 30, "produce: 發明 or 造船 halves the period (documented)");
    TEST_CHECK(counts, sim_produce_days(100, 30, 90, true, &rules) == 20, "produce: a half period goes up to whole turns");
    TEST_CHECK(counts, sim_produce_days(0, 0, 0, false, &rules) == 0, "produce: a smithy weapon has no period");
    TEST_CHECK(counts, sim_weapon_cost(700, false, &rules) == 700 && sim_weapon_cost(700, true, &rules) == 560,
               "produce: a specialty city pays less (assumed 80 percent)");
}

/* A synthetic city with a market and a farm: the income forecast with
   public order and the income skills of skills.md. */
internal void
unit_economy_forecast_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = PushStruct(arena, sim_state);
    sim_init(sim, arena, 1);
    sim_content content = {};
    content.facility_count = 2;
    content.facilities[0].effect = FACILITY_EFFECT_GOLD;  content.facilities[0].value = 50;  content.facilities[0].adjacency_bonus = true;
    content.facilities[1].effect = FACILITY_EFFECT_GRAIN; content.facilities[1].value = 250; content.facilities[1].adjacency_bonus = true;
    content.city_count = 1;
    content.cities[0].slot_count = 12;
    content.rules = unit_economy_rules();
    sim_set_content(sim, &content);
    city_component city = {};
    city.owner_force_id = 1;
    city.public_order   = 100;
    city.troops         = 1250;
    sim_add_city(sim, city);
    sim->sites[0].slots[0].state = SIM_SLOT_BUILT; sim->sites[0].slots[0].facility = 0; sim->sites[0].slots[0].level = 1;
    sim->sites[0].slots[1].state = SIM_SLOT_BUILT; sim->sites[0].slots[1].facility = 1; sim->sites[0].slots[1].level = 1;

    officer_component worker = {};
    worker.officer_id = 7; worker.force_id = 1; worker.status = 3; worker.base_location = 0; worker.current_position = 0;
    worker.skill_number = 255; worker.job_city = SIM_NO_CITY;
    sim_add_officer(sim, worker);

    sim_income_forecast forecast = sim_city_forecast(sim, 0);
    TEST_CHECK(counts, forecast.gold_base == 100 && forecast.gold_facilities == 50 && forecast.gold_month == 150 &&
                       forecast.gold_payment == 150 && !forecast.gold_every_turn,
               "gold each month = the initial income plus the facilities (guide book p.24)");
    TEST_CHECK(counts, forecast.grain_base == 500 && forecast.grain_facilities == 250 && forecast.grain_season == 750 &&
                       forecast.grain_payment == 750 && !forecast.grain_every_month, "grain each season = the initial income plus the farms");
    TEST_CHECK(counts, forecast.salaries == 10 && forecast.grain_per_turn == 12, "the salaries of one officer and the grain of 1250 soldiers");

    sim->cities.data[0].public_order = 60;
    forecast = sim_city_forecast(sim, 0);
    TEST_CHECK(counts, forecast.order_percent == 80 && forecast.gold_month == 120 && forecast.grain_season == 600,
               "public order 60 gives 80 percent of the income (assumed rule)");
    sim->cities.data[0].public_order = 100;

    sim->officers.data[0].skill_number = 87;   /* 富豪, skills.md #88 (assumed: the save number is # - 1) */
    forecast = sim_city_forecast(sim, 0);
    TEST_CHECK(counts, forecast.gold_skill_percent == 150 && forecast.gold_month == 225 && forecast.grain_season == 750,
               "富豪: the gold income of the officer's city is 1.5 times");
    sim->officers.data[0].skill_number = 88;   /* 米道 */
    forecast = sim_city_forecast(sim, 0);
    TEST_CHECK(counts, forecast.grain_skill_percent == 150 && forecast.grain_season == 1125 && forecast.gold_month == 150,
               "米道: the grain income of the officer's city is 1.5 times");
    sim->officers.data[0].skill_number = 89;   /* 徵稅 */
    forecast = sim_city_forecast(sim, 0);
    TEST_CHECK(counts, forecast.gold_every_turn && forecast.gold_payment == 75 && forecast.gold_month == 225,
               "徵稅: gold every turn, each payment halved: 1.5 times in a month");
    sim->officers.data[0].skill_number = 90;   /* 徵收 */
    forecast = sim_city_forecast(sim, 0);
    TEST_CHECK(counts, forecast.grain_every_month && forecast.grain_payment == 375 && forecast.grain_season == 1125,
               "徵收: grain every month, each payment halved: 1.5 times in a season");
    sim->officers.data[0].current_position = 5;
    forecast = sim_city_forecast(sim, 0);
    TEST_CHECK(counts, !forecast.grain_every_month, "a skill applies only to the city where the officer is");
    sim->officers.data[0].current_position = 0;
    sim->officers.data[0].force_id = 2;
    TEST_CHECK(counts, !sim_city_has_skill(sim, 0, 90), "a skill of an officer of another force does not apply");

    TEST_CHECK(counts, sim_event_resource(SIM_EVENT_GOLD_INCOME) == SIM_RESOURCE_GOLD &&
                       sim_event_resource(SIM_EVENT_SALARIES_PAID) == SIM_RESOURCE_GOLD &&
                       sim_event_resource(SIM_EVENT_GOLD_SPENT) == SIM_RESOURCE_GOLD &&
                       sim_event_resource(SIM_EVENT_GOLD_OVER_CAP) == SIM_RESOURCE_GOLD &&
                       sim_event_resource(SIM_EVENT_GOLD_EARNED) == SIM_RESOURCE_GOLD,
               "each event that changes gold names the gold");
    TEST_CHECK(counts, sim_event_resource(SIM_EVENT_GRAIN_INCOME) == SIM_RESOURCE_GRAIN &&
                       sim_event_resource(SIM_EVENT_GRAIN_CONSUMED) == SIM_RESOURCE_GRAIN &&
                       sim_event_resource(SIM_EVENT_GRAIN_BOUGHT) == SIM_RESOURCE_GRAIN &&
                       sim_event_resource(SIM_EVENT_GRAIN_SOLD) == SIM_RESOURCE_GRAIN &&
                       sim_event_resource(SIM_EVENT_GRAIN_OVER_CAP) == SIM_RESOURCE_GRAIN,
               "each event that changes grain names the grain");
    TEST_CHECK(counts, sim_event_resource(SIM_EVENT_CONSCRIPTED) == SIM_RESOURCE_SOLDIERS &&
                       sim_event_resource(SIM_EVENT_TRAINED) == SIM_RESOURCE_MORALE &&
                       sim_event_resource(SIM_EVENT_PATROLLED) == SIM_RESOURCE_PUBLIC_ORDER &&
                       sim_event_resource(SIM_EVENT_WEAPONS_PRODUCED) == SIM_RESOURCE_WEAPONS &&
                       sim_event_resource(SIM_EVENT_FACILITY_REPAIRED) == SIM_RESOURCE_DURABILITY &&
                       sim_event_resource(SIM_EVENT_TECHNIQUE_GAINED) == SIM_RESOURCE_TECHNIQUE &&
                       sim_event_resource(SIM_EVENT_SALARIES_UNPAID) == SIM_RESOURCE_NONE,
               "the other events name their resource; an unpaid salary changes no gold");
    TEST_CHECK(counts, strcmp(sim_resource_name(SIM_RESOURCE_GRAIN), "grain") == 0 &&
                       strcmp(sim_resource_name(SIM_RESOURCE_COUNT), "?") == 0, "each resource has a name");
}

internal void
unit_weapon_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    char *text = content_test_text(arena, "id\tkey\tcost\tfacility\tspecialty\tstock\tmin_days\tmax_days\tneeds_technique\n"
                                          "0\tpike\t700\tweapons\tpike\t0\t0\t0\tno\n"
                                          "1\tram\t1500\tsiege\tsiege\t4\t30\t90\tno\n"
                                          "2\twood_beast\t1700\tsiege\tsiege\t4\t30\t90\tyes\n");
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_weapons(&content, &table, &bad_row) && content.weapon_count == 3,
               "the weapon table parses");
    TEST_CHECK(counts, content.weapons[0].cost == 700 && content.weapons[0].facility == FACILITY_EFFECT_WEAPONS &&
                       content.weapons[0].specialty == CITY_SPECIALTY_PIKE && content.weapons[0].max_days == 0,
               "a smithy weapon arrives at once");
    TEST_CHECK(counts, content.weapons[1].facility == FACILITY_EFFECT_SIEGE && content.weapons[1].min_days == 30 &&
                       content.weapons[1].max_days == 90 && !content.weapons[1].needs_technique && content.weapons[2].needs_technique,
               "a siege weapon has a period and can need a technique");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, "id\tkey\tcost\tfacility\tspecialty\tstock\tmin_days\tmax_days\tneeds_technique\n"
                                              "0\tpike\t700\tsword\tpike\t0\t0\t0\tno\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_weapons(&content, &bad, &bad_row) && bad_row == 0 &&
                       content.weapon_count == 0, "a facility name that is not an effect is a bad row");
    tsv_table outside = {};
    char *outside_text = content_test_text(arena, "id\tkey\tcost\tfacility\tspecialty\tstock\tmin_days\tmax_days\tneeds_technique\n"
                                                  "0\tpike\t700\tweapons\tpike\t10\t0\t0\tno\n");
    TEST_CHECK(counts, tsv_parse(outside_text, arena, &outside) && !content_load_weapons(&content, &outside, &bad_row),
               "a stock index out of the weapon stocks is a bad row");
}

internal void
unit_economy_tests(test_counts *counts, memory_arena *arena)
{
    unit_economy_arithmetic_tests(counts);
    unit_city_command_rule_tests(counts);
    unit_economy_forecast_tests(counts, arena);
    unit_weapon_table_tests(counts, arena);
}
