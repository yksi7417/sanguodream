/* Unit tests of code/sim/develop.cpp: the period from the sum of politics,
   the placeholder slot layout, and the adjacency bonus of the mint and the
   granary (cities-and-facilities.md). */

internal void
unit_develop_tests(test_counts *counts, memory_arena *arena)
{
    sim_rules rules = {};
    rules.develop_max_days      = 100;
    rules.develop_full_politics = 100;
    rules.adjacency_percent     = 150;

    /* The manual's example (p.11): one officer with the politics of 曹操
       (94) needs 30 days; three good officers need 20 days; officers with
       low politics need 60 days. The line between them is assumed (Q-142). */
    TEST_CHECK(counts, sim_develop_days(94, 20, &rules) == 30, "one officer of politics 94 needs 30 days for a market");
    TEST_CHECK(counts, sim_develop_days(94 + 95 + 85, 20, &rules) == 20, "three good officers need the shortest period, 20 days");
    TEST_CHECK(counts, sim_develop_days(50, 20, &rules) == 60, "a low politics sum of 50 needs 60 days");
    TEST_CHECK(counts, sim_develop_days(0, 20, &rules) == 100, "a politics sum of 0 needs the longest period, 100 days");
    TEST_CHECK(counts, sim_develop_days(100, 30, &rules) == 30 && sim_develop_days(400, 30, &rules) == 30,
               "a sum at or above the full politics gives the shortest period of the facility");
    u32 previous = sim_develop_days(0, 30, &rules);
    b32 never_longer = true;
    for (u32 sum = 1; sum <= 300; ++sum)
    {
        u32 days = sim_develop_days(sum, 30, &rules);
        if (days > previous || days % CALENDAR_DAYS_PER_TURN != 0) never_longer = false;
        previous = days;
    }
    TEST_CHECK(counts, never_longer, "more politics never gives a longer period; a period is whole turns");

    /* The placeholder layout: rings of hexes around the city. */
    b32 ring_one = true, ring_two = true, distinct = true;
    for (u32 slot = 0; slot < SIM_SLOT_CAPACITY; ++slot)
    {
        hex_coord hex = sim_city_slot_hex(slot);
        s32 ring = hex_distance(hex, hex_make(0, 0));
        if (slot < 6 && ring != 1) ring_one = false;
        if (slot >= 6 && slot < 18 && ring != 2) ring_two = false;
        for (u32 other = 0; other < slot; ++other)
        {
            if (hex_equal(hex, sim_city_slot_hex(other))) distinct = false;
        }
    }
    TEST_CHECK(counts, ring_one && ring_two, "slots 0 to 5 are the first ring, slots 6 to 17 the second ring");
    TEST_CHECK(counts, distinct, "no two slots share a hex");

    /* The adjacency bonus on a synthetic city. */
    sim_state *sim = PushStruct(arena, sim_state);
    sim_init(sim, arena, 1);
    sim_content content = {};
    content.facility_count = 3;
    content.facilities[0].effect = FACILITY_EFFECT_GOLD;  content.facilities[0].value = 50;  content.facilities[0].adjacency_bonus = true;
    content.facilities[1].effect = FACILITY_EFFECT_MINT;
    content.facilities[2].effect = FACILITY_EFFECT_GOLD;  content.facilities[2].value = 150;   /* a great market: no bonus */
    content.city_count = 1;
    content.cities[0].slot_count = 20;
    content.rules = rules;
    content.rules.city_gold_income = 100;
    sim_set_content(sim, &content);
    city_component city = {};
    city.owner_force_id = 1;
    sim_add_city(sim, city);

    sim_city_site *site = sim->sites + 0;
    u32 mint_slot = 0;
    u32 adjacent = SIM_SLOT_CAPACITY, distant = SIM_SLOT_CAPACITY, great = SIM_SLOT_CAPACITY;
    for (u32 slot = 1; slot < site->slot_count; ++slot)
    {
        s32 distance = hex_distance(site->slots[slot].hex, site->slots[mint_slot].hex);
        if (distance == 1 && adjacent == SIM_SLOT_CAPACITY) adjacent = slot;
        else if (distance == 1 && great == SIM_SLOT_CAPACITY) great = slot;
        else if (distance > 1 && distant == SIM_SLOT_CAPACITY) distant = slot;
    }
    site->slots[adjacent].state = SIM_SLOT_BUILT; site->slots[adjacent].facility = 0;
    site->slots[distant].state      = SIM_SLOT_BUILT; site->slots[distant].facility      = 0;
    site->slots[great].state    = SIM_SLOT_BUILT; site->slots[great].facility    = 2;
    facility_effect effect = FACILITY_EFFECT_NONE;
    TEST_CHECK(counts, sim_slot_yield(sim, 0, adjacent, &effect) == 50 && effect == FACILITY_EFFECT_GOLD,
               "a market without a mint gives its value");
    TEST_CHECK(counts, sim_city_gold_income(sim, 0) == 100 + 50 + 50 + 150, "the gold income is the city value plus the facilities");

    site->slots[mint_slot].state = SIM_SLOT_BUILDING; site->slots[mint_slot].facility = 1;
    TEST_CHECK(counts, sim_slot_yield(sim, 0, adjacent, &effect) == 50, "a mint under construction gives no bonus (assumed)");
    site->slots[mint_slot].state = SIM_SLOT_BUILT;
    TEST_CHECK(counts, sim_slot_yield(sim, 0, adjacent, &effect) == 75, "a market next to a mint gives 1.5 times its value");
    TEST_CHECK(counts, sim_slot_yield(sim, 0, distant, &effect) == 50, "a market away from the mint gives its value");
    TEST_CHECK(counts, sim_slot_yield(sim, 0, great, &effect) == 150, "a great market next to a mint gets no bonus");
    TEST_CHECK(counts, sim_slot_yield(sim, 0, mint_slot, &effect) == 0, "the mint itself gives no gold");
    TEST_CHECK(counts, sim_city_gold_income(sim, 0) == 100 + 75 + 50 + 150, "the city income has the bonus");
    TEST_CHECK(counts, sim_city_used_slots(sim, 0) == 4, "a slot with a facility, built or under construction, is used");
}
