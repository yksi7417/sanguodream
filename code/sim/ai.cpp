/* The AI planner of M8 (knowledge/ai.md, diplomacy.md): a computer faction
   and a delegated corps issue the same commands as a player. The planner
   reads ai_priorities.tsv: each row proposes one kind of command when its
   condition holds. A proposal must pass the check of its command; the
   highest score goes first. The council (評定) and the adviser (軍師) show
   the same proposals. Units move toward the nearest hostile city and
   attack what stands next to them. Included by sim.cpp after events.cpp.

   The spec describes the behaviour of the original AI only as player
   claims; the table, the urgency and the unit rule are assumed (Q-185). */

/* ---- one proposal ---------------------------------------------------------------- */

/* The free officer of the city with the best ability, or SIM_NO_OFFICER. */
internal u16
sim_ai_best_officer(sim_state *sim, city_component *city, sim_ability ability, u16 exclude)
{
    officer_component *best = 0;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->officer_id == exclude || sim_check_develop_officer(sim, city, officer) != SIM_REFUSAL_NONE) continue;
        if (officer->status == OFFICER_STATUS_RULER && sim->turns.human[city->owner_force_id]) continue;
        if (!best || sim_officer_ability(officer, ability) > sim_officer_ability(best, ability)) best = officer;
    }
    return best ? best->officer_id : (u16)SIM_NO_OFFICER;
}

internal sim_city_order
sim_ai_city_order(city_component *city, u16 officer)
{
    sim_city_order order = {};
    order.city_id     = city->city_id;
    order.officers[0] = officer;
    order.officers[1] = order.officers[2] = SIM_NO_OFFICER;
    return order;
}

internal sim_court_order
sim_ai_court_order(city_component *city, u16 officer)
{
    sim_court_order order = {};
    order.city_id     = city->city_id;
    order.officer     = officer;
    order.target      = SIM_NO_OFFICER;
    order.item        = 0xFF;
    order.post        = CONTENT_NO_POST;
    order.force       = order.force_2 = SIM_NO_FORCE;
    order.target_city = SIM_NO_CITY;
    return order;
}

/* M15: the months from the start date of the scenario to the date of the game. */
internal u32
sim_ai_months_played(sim_state *sim)
{
    s32 start = (s32)sim->turns.start_date.year * 12 + sim->turns.start_date.month;
    s32 now   = (s32)sim->turns.date.year * 12 + sim->turns.date.month;
    return now > start ? (u32)(now - start) : 0;
}

/* M15: a city without an owner within ai.target_hexes of the city. */
internal b32
sim_ai_free_city_near(sim_state *sim, city_component *city)
{
    hex_coord from = sim->sites[city->city_id].hex;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *other = sim->cities.data + index;
        if (other->owner_force_id != SIM_NO_FORCE) continue;
        if ((u32)hex_distance(sim->sites[other->city_id].hex, from) <= sim->content.rules.ai_target_hexes) return true;
    }
    return false;
}

/* The urgency of a condition, 0 .. ai.urgency, and whether it holds. */
internal b32
sim_ai_condition(sim_state *sim, city_component *city, ai_priority *row, u32 *urgency)
{
    sim_rules *rules = &sim->content.rules;
    u32 scale = rules->ai_urgency;
    *urgency = 0;
    switch (row->condition)
    {
        case AI_ORDER_BELOW:
            if (city->public_order >= row->threshold) return false;
            *urgency = row->threshold ? scale * (row->threshold - city->public_order) / row->threshold : 0;
            return true;
        case AI_TROOPS_BELOW_PERCENT:
        {
            u32 wanted = sim_percent_of(sim_city_soldier_cap(sim, city->city_id), row->threshold);
            if (city->troops >= wanted) return false;
            *urgency = wanted ? scale * (wanted - city->troops) / wanted : 0;
            return true;
        }
        case AI_FREE_SLOT:
            return sim_city_used_slots(sim, city->city_id) < sim->sites[city->city_id].slot_count;
        case AI_FREE_OFFICER:
            for (u32 index = 0; index < sim->officers.count; ++index)
            {
                officer_component *officer = sim->officers.data + index;
                if (officer->status == OFFICER_STATUS_FREE_OR_NOT_APPEARED && officer->base_location == city->city_id &&
                    officer->appearance_year <= sim->turns.date.year) return true;
            }
            return false;
        case AI_MORALE_BELOW:
            if (!city->troops || city->morale >= row->threshold) return false;
            *urgency = scale * (row->threshold - city->morale) / Maximum(row->threshold, 1u);
            return true;
        case AI_WEAPONS_BELOW_TROOPS:
        {
            u32 weapons = 0;
            for (u32 stock = 0; stock < 4; ++stock) weapons += city->equipment_stocks[stock];
            return weapons < city->troops;
        }
        case AI_LOYALTY_BELOW:
            for (u32 index = 0; index < sim->officers.count; ++index)
            {
                officer_component *officer = sim->officers.data + index;
                if (sim_officer_is_active(officer) && officer->force_id == city->owner_force_id && officer->current_position == city->city_id &&
                    officer->status != OFFICER_STATUS_RULER && officer->loyalty < row->threshold) return true;
            }
            return false;
        case AI_TROOPS_ABOVE:    return city->troops > row->threshold;
        case AI_TECHNIQUE_ABOVE: return sim->technique_points[city->owner_force_id] >= row->threshold;
        case AI_GOLD_ABOVE:      return city->gold > row->threshold;
        case AI_ENEMY_NEAR:
            /* M15: an enemy unit within enemy.block_hexes of the city; full urgency. */
            if (!sim_enemy_near(sim, city)) return false;
            *urgency = scale;
            return true;
        case AI_FREE_CITY_NEAR:  return sim_ai_free_city_near(sim, city);
        default:                 return false;
    }
}

/* U-012 (the play run of 2026-11-07): units of 2000 soldiers marched on
   cities of 30000 and died on the way. The strength of a force near a
   target: the soldiers that each own city within ai.target_hexes can send
   (at most ai.sortie_soldiers above ai.sortie_reserve), and the soldiers
   of its units within that distance. Several cities thus march on one
   target, and the AI does not attack with one unit at a time
   (knowledge/ai.md). */
internal u64
sim_ai_strength_near(sim_state *sim, u8 force, hex_coord target)
{
    sim_rules *rules = &sim->content.rules;
    u64 strength = 0;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id != force || city->troops <= rules->ai_sortie_reserve) continue;
        if ((u32)hex_distance(sim->sites[city->city_id].hex, target) > rules->ai_target_hexes) continue;
        strength += Minimum(rules->ai_sortie_soldiers, city->troops - rules->ai_sortie_reserve);
    }
    for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
    {
        sim_unit *unit = sim->units + unit_id;
        if (!unit->alive || unit->force_id != force) continue;
        if ((u32)hex_distance(unit->hex, target) <= rules->ai_target_hexes) strength += unit->soldiers;
    }
    return strength;
}

/* True if the force marches on the city: its troops are at most
   ai.attack_troop_percent of the strength of the force near it. A city
   without an owner is always a target. */
internal b32
sim_ai_city_beatable(sim_state *sim, city_component *city, u8 force)
{
    if (city->owner_force_id == SIM_NO_FORCE) return true;
    u64 strength = sim_ai_strength_near(sim, force, sim->sites[city->city_id].hex);
    return (u64)city->troops * 100 <= strength * sim->content.rules.ai_attack_troop_percent;
}

/* The land unit type of a sortie: the type with the most weapons in the
   city. Before U-012 the AI always took pikes, and a city with few pikes
   sent a small unit. */
internal u8
sim_ai_sortie_type(sim_state *sim, city_component *city)
{
    static const char *keys[] = { "pike", "halberd", "crossbow", "cavalry" };
    u8 best = CONTENT_NO_UNIT_TYPE;
    u32 best_stock = 0;
    for (u32 index = 0; index < ArrayCount(keys); ++index)
    {
        u8 type = content_find_unit_type(&sim->content, keys[index]);
        if (type == CONTENT_NO_UNIT_TYPE) continue;
        u8 weapon = sim->content.unit_types[type].weapon;
        if (weapon >= sim->content.weapon_count) continue;
        u32 stock = city->equipment_stocks[sim->content.weapons[weapon].stock];
        if (best == CONTENT_NO_UNIT_TYPE || stock > best_stock) { best = type; best_stock = stock; }
    }
    return best;
}

/* The nearest hostile city of the force, or SIM_NO_CITY. M15: a city
   without an owner is a target too. */
internal u8
sim_ai_target_city(sim_state *sim, u8 force, hex_coord from, u32 max_distance)
{
    u8 best = SIM_NO_CITY;
    s32 best_distance = 0x7FFFFFFF;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *other = sim->cities.data + index;
        if (!sim_hostile(sim, force, other->owner_force_id)) continue;
        if (!sim_ai_city_beatable(sim, other, force)) continue;
        s32 distance = hex_distance(sim->sites[other->city_id].hex, from);
        if ((u32)distance <= max_distance && distance < best_distance) { best = other->city_id; best_distance = distance; }
    }
    return best;
}

/* The play run of 2026-11-07: an AI city that always took the first
   effect that it can build filled its slots with markets. It had no barracks,
   and it never conscripted. The city now builds toward the targets
   ai.develop_*: the template of a delegated corps in knowledge/ai.md
   (3 markets, 3 farms, 1 barracks, 1 smithy). */
#define SIM_AI_DEVELOP_CHOICES 4

/* The slots of the city with a facility of the effect, built or in work. */
internal u32
sim_ai_facilities_with(sim_state *sim, u8 city_id, facility_effect effect)
{
    u32 count = 0;
    sim_city_site *site = sim->sites + city_id;
    for (u32 slot = 0; slot < site->slot_count; ++slot)
    {
        u8 facility = site->slots[slot].facility;
        if (site->slots[slot].state == SIM_SLOT_EMPTY || facility >= sim->content.facility_count) continue;
        if (sim->content.facilities[facility].effect == effect) count++;
    }
    return count;
}

/* True if the grain of the city feeds the soldiers for ai.grain_turns turns. */
internal b32
sim_ai_grain_lasts(sim_state *sim, city_component *city, u32 soldiers)
{
    sim_rules *rules = &sim->content.rules;
    u64 per_turn = rules->grain_soldiers ? soldiers / rules->grain_soldiers : 0;
    return (u64)city->food >= per_turn * rules->ai_grain_turns;
}

/* The effects that the city builds, the first choice first. A city that
   is short of grain builds grain first. First one
   facility of each effect, then the effect with the largest gap to its
   target. After the targets, the fewer of gold and grain. */
internal u32
sim_ai_develop_order(sim_state *sim, u8 city_id, facility_effect *order)
{
    sim_rules *rules = &sim->content.rules;
    /* U-012: the smithy before the farm. Without weapons the soldiers of the barracks cannot march. */
    static const facility_effect effects[SIM_AI_DEVELOP_CHOICES] = { FACILITY_EFFECT_GOLD, FACILITY_EFFECT_CONSCRIPTION, FACILITY_EFFECT_WEAPONS, FACILITY_EFFECT_GRAIN };
    u32 targets[SIM_AI_DEVELOP_CHOICES] = { rules->ai_develop_gold, rules->ai_develop_conscription, rules->ai_develop_weapons, rules->ai_develop_grain };
    s32 gap[SIM_AI_DEVELOP_CHOICES];
    for (u32 index = 0; index < SIM_AI_DEVELOP_CHOICES; ++index)
    {
        u32 have = sim_ai_facilities_with(sim, city_id, effects[index]);
        gap[index] = (s32)targets[index] - (s32)have;
        /* A missing effect goes before every gap. */
        if (targets[index] && !have) gap[index] += 0x10000;
    }
    b32 used[SIM_AI_DEVELOP_CHOICES] = {};
    u32 count = 0;
    city_component *city = sim_find_city(sim, city_id);
    if (city && !sim_ai_grain_lasts(sim, city, city->troops))
    {
        used[3] = true;   /* index 3 is grain */
        order[count++] = FACILITY_EFFECT_GRAIN;
    }
    for (u32 pick = 0; pick < SIM_AI_DEVELOP_CHOICES; ++pick)
    {
        u32 best = SIM_AI_DEVELOP_CHOICES;
        for (u32 index = 0; index < SIM_AI_DEVELOP_CHOICES; ++index)
        {
            if (used[index] || gap[index] <= 0) continue;
            if (best == SIM_AI_DEVELOP_CHOICES || gap[index] > gap[best]) best = index;
        }
        if (best == SIM_AI_DEVELOP_CHOICES) break;
        used[best] = true;
        order[count++] = effects[best];
    }
    if (count) return count;
    b32 grain_first = sim_ai_facilities_with(sim, city_id, FACILITY_EFFECT_GRAIN) < sim_ai_facilities_with(sim, city_id, FACILITY_EFFECT_GOLD);
    order[count++] = grain_first ? FACILITY_EFFECT_GRAIN : FACILITY_EFFECT_GOLD;
    order[count++] = grain_first ? FACILITY_EFFECT_GOLD : FACILITY_EFFECT_GRAIN;
    return count;
}

/* U-015 (the play run of 2026-11-07): the AI sent 1834 envoys of goodwill
   in 15 years and made no alliance. The factions at war that own a city
   within ai.target_hexes of a city of the force: its neighbours. */
internal u32
sim_ai_neighbours(sim_state *sim, u8 force, b32 *is_neighbour)
{
    u32 count = 0;
    for (u32 other = 0; other < SIM_DIPLOMACY_FORCES; ++other) is_neighbour[other] = false;
    for (u32 mine = 0; mine < sim->cities.count; ++mine)
    {
        city_component *own = sim->cities.data + mine;
        if (own->owner_force_id != force) continue;
        for (u32 index = 0; index < sim->cities.count; ++index)
        {
            city_component *other = sim->cities.data + index;
            u8 owner = other->owner_force_id;
            if (owner >= SIM_DIPLOMACY_FORCES || is_neighbour[owner] || !sim_at_war(sim, force, owner)) continue;
            if ((u32)hex_distance(sim->sites[own->city_id].hex, sim->sites[other->city_id].hex) > sim->content.rules.ai_target_hexes) continue;
            is_neighbour[owner] = true;
            count++;
        }
    }
    return count;
}

/* The friendliest neighbour at war with friendliness below the bound, if
   the force has another neighbour. An alliance with the only neighbour
   ends the wars of the force (diplomacy.md, AI behaviour). */
internal u8
sim_ai_friendliest_neighbour(sim_state *sim, u8 force, u32 below)
{
    b32 is_neighbour[SIM_DIPLOMACY_FORCES];
    if (sim_ai_neighbours(sim, force, is_neighbour) < 2) return SIM_NO_FORCE;
    u8 best = SIM_NO_FORCE;
    u32 best_friendliness = 0;
    for (u32 other = 0; other < SIM_DIPLOMACY_FORCES; ++other)
    {
        if (!is_neighbour[other]) continue;
        u32 friendliness = sim_friendliness(sim, force, (u8)other);
        if (friendliness >= below) continue;
        if (best == SIM_NO_FORCE || friendliness > best_friendliness) { best = (u8)other; best_friendliness = friendliness; }
    }
    return best;
}

/* True if an envoy of the force with this kind of diplomacy to the other
   force is on the road. The AI sends no second envoy (U-015). */
internal b32
sim_ai_envoy_on_road(sim_state *sim, u8 force, u8 other, u8 kind)
{
    for (u32 index = 0; index < SIM_MISSION_CAPACITY; ++index)
    {
        sim_mission *mission = sim->missions + index;
        if (mission->active && mission->kind == MISSION_DIPLOMACY && mission->force == force &&
            mission->order.force == other && mission->order.kind == kind) return true;
    }
    return false;
}

/* The order of 同盟 with the friendliest neighbour at war, with the gold of
   alliance.gold. False if no neighbour has the friendliness and the chance. */
internal b32
sim_ai_alliance_order(sim_state *sim, city_component *city, sim_court_order *order)
{
    sim_rules *rules = &sim->content.rules;
    u8 ally = sim_ai_friendliest_neighbour(sim, city->owner_force_id, 101);
    if (ally == SIM_NO_FORCE || sim_friendliness(sim, city->owner_force_id, ally) < rules->ai_alliance_friendliness) return false;
    if (sim_ai_envoy_on_road(sim, city->owner_force_id, ally, SIM_DIPLOMACY_ALLIANCE)) return false;
    /* The ally needs another enemy too. Else it breaks the alliance in its next turn. */
    b32 their_neighbours[SIM_DIPLOMACY_FORCES];
    if (sim_ai_neighbours(sim, ally, their_neighbours) < 2) return false;
    order->kind  = SIM_DIPLOMACY_ALLIANCE;
    order->force = ally;
    order->gold  = rules->alliance_gold;
    sim_check check = sim_check_diplomacy(sim, order);
    return check.refusal == SIM_REFUSAL_NONE && check.quantity >= rules->ai_alliance_chance;
}

/* The order of 摒棄 with an allied neighbour, when the force has no
   neighbour at war: the ally is then its only neighbour. */
internal b32
sim_ai_break_order(sim_state *sim, city_component *city, sim_court_order *order)
{
    u8 force = city->owner_force_id;
    b32 is_neighbour[SIM_DIPLOMACY_FORCES];
    if (sim_ai_neighbours(sim, force, is_neighbour) > 0) return false;
    for (u32 mine = 0; mine < sim->cities.count; ++mine)
    {
        city_component *own = sim->cities.data + mine;
        if (own->owner_force_id != force) continue;
        for (u32 index = 0; index < sim->cities.count; ++index)
        {
            city_component *other = sim->cities.data + index;
            u8 owner = other->owner_force_id;
            if (owner >= SIM_DIPLOMACY_FORCES || owner == force || sim_relation(sim, force, owner) != RELATION_ALLIANCE) continue;
            if (sim->diplomacy.months[force][owner] < sim->content.rules.ai_alliance_min_months) continue;
            if ((u32)hex_distance(sim->sites[own->city_id].hex, sim->sites[other->city_id].hex) > sim->content.rules.ai_target_hexes) continue;
            if (sim_ai_envoy_on_road(sim, force, owner, SIM_DIPLOMACY_BREAK)) return false;
            order->kind  = SIM_DIPLOMACY_BREAK;
            order->force = owner;
            return sim_check_diplomacy(sim, order).refusal == SIM_REFUSAL_NONE;
        }
    }
    return false;
}

/* The command of a row for the city, or kind SIM_COMMAND_NONE if none fits. */
internal sim_command
sim_ai_command_for(sim_state *sim, city_component *city, ai_priority *row)
{
    sim_command command = {};
    command.kind = SIM_COMMAND_NONE;
    sim_command_kind kind = (sim_command_kind)row->command;
    switch (kind)
    {
        case SIM_COMMAND_PATROL:
        case SIM_COMMAND_CONSCRIPT:
        case SIM_COMMAND_TRAIN:
        case SIM_COMMAND_PRODUCE:
        {
            sim_ability ability = kind == SIM_COMMAND_PATROL ? SIM_ABILITY_LEADERSHIP : kind == SIM_COMMAND_CONSCRIPT ? SIM_ABILITY_CHARM :
                                  kind == SIM_COMMAND_TRAIN ? SIM_ABILITY_FORCE : SIM_ABILITY_INTELLECT;
            sim_city_order order = sim_ai_city_order(city, sim_ai_best_officer(sim, city, ability, SIM_NO_OFFICER));
            if (kind == SIM_COMMAND_PRODUCE)
            {
                u32 lowest = 0xFFFFFFFFu;
                for (u32 weapon = 0; weapon < sim->content.weapon_count; ++weapon)
                {
                    weapon_kind *kind_row = sim->content.weapons + weapon;
                    if (kind_row->max_days || kind_row->stock >= CITY_EQUIPMENT_STOCK_COUNT) continue;
                    order.weapon = (u8)weapon;
                    if (sim_check_city_command(sim, kind, &order).refusal != SIM_REFUSAL_NONE) continue;
                    if (city->equipment_stocks[kind_row->stock] < lowest) { lowest = city->equipment_stocks[kind_row->stock]; command.city.weapon = (u8)weapon; }
                }
                if (lowest == 0xFFFFFFFFu) return command;
                order.weapon = command.city.weapon;
            }
            sim_check check = sim_check_city_command(sim, kind, &order);
            if (check.refusal != SIM_REFUSAL_NONE) return command;
            /* The play run of 2026-11-07: the AI conscripted soldiers that its grain did not feed, and they deserted. */
            if (kind == SIM_COMMAND_CONSCRIPT && !sim_ai_grain_lasts(sim, city, city->troops + check.quantity)) return command;
            command.kind = kind;
            command.city = order;
            return command;
        }
        case SIM_COMMAND_DEVELOP:
        {
            sim_develop_order order = {};
            order.city_id     = city->city_id;
            order.slot        = SIM_ANY_SLOT;
            order.officers[0] = sim_ai_best_officer(sim, city, SIM_ABILITY_POLITICS, SIM_NO_OFFICER);
            order.officers[1] = order.officers[2] = SIM_NO_OFFICER;
            facility_effect wanted[SIM_AI_DEVELOP_CHOICES];
            u32 wanted_count = sim_ai_develop_order(sim, city->city_id, wanted);
            for (u32 want = 0; want < wanted_count; ++want)
            {
                for (u32 facility = 0; facility < sim->content.facility_count; ++facility)
                {
                    if (sim->content.facilities[facility].effect != wanted[want] || sim->content.facilities[facility].pk) continue;
                    order.facility = (u8)facility;
                    if (sim_check_develop(sim, &order).refusal != SIM_REFUSAL_NONE) continue;
                    command.kind    = SIM_COMMAND_DEVELOP;
                    command.develop = order;
                    return command;
                }
            }
            return command;
        }
        case SIM_COMMAND_RECRUIT:
        {
            sim_court_order order = sim_ai_court_order(city, sim_ai_best_officer(sim, city, SIM_ABILITY_CHARM, SIM_NO_OFFICER));
            for (u32 index = 0; index < sim->officers.count; ++index)
            {
                officer_component *officer = sim->officers.data + index;
                if (officer->status != OFFICER_STATUS_FREE_OR_NOT_APPEARED || officer->base_location != city->city_id) continue;
                order.target = officer->officer_id;
                if (sim_check_court(sim, SIM_COMMAND_RECRUIT, &order).refusal != SIM_REFUSAL_NONE) continue;
                command.kind  = SIM_COMMAND_RECRUIT;
                command.court = order;
                return command;
            }
            return command;
        }
        case SIM_COMMAND_REWARD:
        {
            /* The AI rewards up to three officers of the city in one order. */
            sim_court_order order = sim_ai_court_order(city, SIM_NO_OFFICER);
            for (u32 index = 0; index < sim->officers.count && order.reward_count < SIM_COMMAND_OFFICERS; ++index)
            {
                officer_component *officer = sim->officers.data + index;
                if (officer->current_position == city->city_id && officer->loyalty < row->threshold &&
                    sim_check_reward_officer(sim, city->owner_force_id, officer) == SIM_REFUSAL_NONE)
                {
                    order.rewards[order.reward_count++] = officer->officer_id;
                }
            }
            if (!order.reward_count || sim_check_court(sim, SIM_COMMAND_REWARD, &order).refusal != SIM_REFUSAL_NONE) return command;
            command.kind  = SIM_COMMAND_REWARD;
            command.court = order;
            return command;
        }
        case SIM_COMMAND_RESEARCH:
        {
            sim_research_order order = {};
            order.city_id     = city->city_id;
            order.officers[0] = sim_ai_best_officer(sim, city, SIM_ABILITY_INTELLECT, SIM_NO_OFFICER);
            order.officers[1] = order.officers[2] = SIM_NO_OFFICER;
            for (u32 technique = 0; technique < sim->content.technique_count; ++technique)
            {
                order.technique = (u8)technique;
                if (sim_check_research(sim, &order).refusal != SIM_REFUSAL_NONE) continue;
                command.kind     = SIM_COMMAND_RESEARCH;
                command.research = order;
                return command;
            }
            return command;
        }
        case SIM_COMMAND_SORTIE:
        {
            sim_rules *rules = &sim->content.rules;
            /* The row "defend" sends the unit against the enemy near the city; the other rows need a target city. */
            /* E7: a target of a historical event is a target at any distance. */
            if (row->condition != AI_ENEMY_NEAR && sim_history_target(sim, city->owner_force_id) == SIM_NO_CITY &&
                sim_ai_target_city(sim, city->owner_force_id, sim->sites[city->city_id].hex, rules->ai_target_hexes) == SIM_NO_CITY) return command;
            /* M15: no march on a city in the first months of the game (knowledge/ai.md: the AI armies
               arrive 5 to 6 months after the start; assumed, Q-185). Only the defence does not wait. */
            if (row->condition != AI_ENEMY_NEAR && sim_ai_months_played(sim) < rules->ai_attack_after_months) return command;
            u8 type = sim_ai_sortie_type(sim, city);
            if (type == CONTENT_NO_UNIT_TYPE || city->troops <= rules->ai_sortie_reserve) return command;
            sim_sortie_order order = sim_sortie_proposal(sim, city->city_id, type);
            order.officers[1] = order.officers[2] = SIM_NO_OFFICER;   /* one officer: the others stay to govern */
            order.soldiers = Minimum(Minimum(order.soldiers, rules->ai_sortie_soldiers), city->troops - rules->ai_sortie_reserve);
            order.grain    = Minimum(order.grain, city->food);
            order.gold     = 0;
            if (sim_check_sortie(sim, &order, false).refusal != SIM_REFUSAL_NONE) return command;
            command.kind   = SIM_COMMAND_SORTIE;
            command.sortie = order;
            return command;
        }
        case SIM_COMMAND_DIPLOMACY:
        {
            sim_court_order order = sim_ai_court_order(city, sim_ai_best_officer(sim, city, SIM_ABILITY_POLITICS, SIM_NO_OFFICER));
            b32 alliance = strcmp(row->key, "alliance") == 0, breaks = strcmp(row->key, "break") == 0;
            if (alliance || breaks)
            {
                if (alliance ? !sim_ai_alliance_order(sim, city, &order) : !sim_ai_break_order(sim, city, &order)) return command;
                command.kind  = SIM_COMMAND_DIPLOMACY;
                command.court = order;
                return command;
            }
            order.kind = SIM_DIPLOMACY_GOODWILL;
            u32 lowest = 101;
            for (u32 other = 0; other < SIM_DIPLOMACY_FORCES; ++other)
            {
                if (other == city->owner_force_id || sim_force_ruler(sim, (u8)other) == SIM_NO_OFFICER) continue;
                u32 friendliness = sim_friendliness(sim, city->owner_force_id, (u8)other);
                if (friendliness < lowest) { lowest = friendliness; order.force = (u8)other; }
            }
            /* U-015: goodwill goes to the friendliest neighbour at war, a future ally, when the
               faction has two neighbours or more. Else to the faction with the lowest friendliness. */
            u8 neighbour = sim_ai_friendliest_neighbour(sim, city->owner_force_id, 100);
            if (neighbour != SIM_NO_FORCE) { order.force = neighbour; lowest = 0; }
            if (lowest > 100 || sim_check_diplomacy(sim, &order).refusal != SIM_REFUSAL_NONE) return command;
            command.kind  = SIM_COMMAND_DIPLOMACY;
            command.court = order;
            return command;
        }
        default:
            return command;
    }
}

/* The commands that a corps setting allows. */
internal b32
sim_ai_allowed(sim_command_kind kind, u32 flags)
{
    switch (kind)
    {
        case SIM_COMMAND_PRODUCE:   return (flags & CORPS_FLAG_PRODUCE) != 0;
        case SIM_COMMAND_CONSCRIPT: return (flags & CORPS_FLAG_CONSCRIPT) != 0;
        case SIM_COMMAND_RECRUIT:   return (flags & CORPS_FLAG_RECRUIT) != 0;
        case SIM_COMMAND_SORTIE:    return (flags & CORPS_FLAG_ATTACK) != 0;
        case SIM_COMMAND_DEVELOP:
        case SIM_COMMAND_RESEARCH:  return (flags & CORPS_FLAG_SAVE_GOLD) == 0;
        default:                    return true;
    }
}

/* The lowest gold of one production of a weapon of the smithy. */
internal u32
sim_ai_weapon_cost(sim_state *sim)
{
    u32 lowest = 0;
    for (u32 weapon = 0; weapon < sim->content.weapon_count; ++weapon)
    {
        weapon_kind *kind = sim->content.weapons + weapon;
        if (kind->facility != FACILITY_EFFECT_WEAPONS || kind->max_days) continue;
        if (!lowest || kind->cost < lowest) lowest = kind->cost;
    }
    return lowest;
}

/* The gold that the command of a proposal spends from its city, by the
   check of the command. 0 for a command without gold. */
internal u32
sim_ai_command_gold(sim_state *sim, sim_command *command)
{
    switch (command->kind)
    {
        case SIM_COMMAND_PATROL:
        case SIM_COMMAND_CONSCRIPT:
        case SIM_COMMAND_TRAIN:
        case SIM_COMMAND_PRODUCE:   return sim_check_city_command(sim, command->kind, &command->city).gold;
        case SIM_COMMAND_DEVELOP:   return sim_check_develop(sim, &command->develop).gold;
        case SIM_COMMAND_RESEARCH:  return sim_check_research(sim, &command->research).gold;
        case SIM_COMMAND_RECRUIT:
        case SIM_COMMAND_REWARD:    return sim_check_court(sim, command->kind, &command->court).gold;
        case SIM_COMMAND_DIPLOMACY: return sim_check_diplomacy(sim, &command->court).gold;
        default:                    return 0;
    }
}

/* U-011 (the play run of 2026-11-07): the AI spent its gold on facilities,
   missed the salaries, and its officers left. A city keeps the salaries of
   ai.salary_months months. The defence of the city does not wait. */
internal b32
sim_ai_keeps_salaries(sim_state *sim, city_component *city, ai_priority *row, sim_command *command)
{
    if (row->condition == AI_ENEMY_NEAR) return true;
    u64 reserve = (u64)sim_city_salaries(sim, city) * sim->content.rules.ai_salary_months;
    /* U-012: a city with a smithy and fewer weapons than soldiers saves the gold of one
       production before it builds, researches or sends an envoy. */
    b32 saves = command->kind == SIM_COMMAND_DEVELOP || command->kind == SIM_COMMAND_RESEARCH || command->kind == SIM_COMMAND_DIPLOMACY;
    if (saves && sim_ai_facilities_with(sim, city->city_id, FACILITY_EFFECT_WEAPONS))
    {
        u32 weapons = 0;
        for (u32 stock = 0; stock < 4; ++stock) weapons += city->equipment_stocks[stock];
        if (weapons < city->troops) reserve += sim_ai_weapon_cost(sim);
    }
    return (u64)city->gold >= reserve + sim_ai_command_gold(sim, command);
}

/* The proposals for the cities of a force: all of them, one city, or the
   cities of one corps. Sorted by score, the highest first. */
internal void
sim_plan_faction(sim_state *sim, u8 force, u8 city_filter, u8 corps_filter, u32 flags, sim_plan *plan)
{
    plan->count = 0;
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id != force) continue;
        if (city_filter != SIM_NO_CITY && city->city_id != city_filter) continue;
        if (corps_filter != 0xFF && city->corps != corps_filter) continue;
        for (u32 row = 0; row < sim->content.priority_count; ++row)
        {
            ai_priority *priority = sim->content.priorities + row;
            if (!sim_ai_allowed((sim_command_kind)priority->command, flags)) continue;
            if (priority->command == SIM_COMMAND_DIPLOMACY && strcmp(priority->key, "alliance") != 0 && strcmp(priority->key, "break") != 0 &&
                city->gold < rules->ai_goodwill_gold) continue;
            u32 urgency = 0;
            if (!sim_ai_condition(sim, city, priority, &urgency)) continue;
            sim_command command = sim_ai_command_for(sim, city, priority);
            if (command.kind == SIM_COMMAND_NONE || plan->count >= SIM_PLAN_CAPACITY) continue;
            if (!sim_ai_keeps_salaries(sim, city, priority, &command)) continue;
            sim_proposal proposal = {};
            proposal.command = command;
            proposal.score   = priority->priority + Minimum(urgency, rules->ai_urgency);
            proposal.row     = (u8)row;
            u32 at = plan->count++;
            while (at > 0 && plan->items[at - 1].score < proposal.score) { plan->items[at] = plan->items[at - 1]; --at; }
            plan->items[at] = proposal;
        }
    }
}

/* ---- the record of an AI turn (M15, ai_explain) ----------------------------------- */

/* The city and the first officer of a command, for a front end. */
internal void
sim_command_place(sim_command *command, u8 *city_id, u16 *officer)
{
    *city_id = SIM_NO_CITY;
    *officer = SIM_NO_OFFICER;
    switch (command->kind)
    {
        case SIM_COMMAND_PATROL:
        case SIM_COMMAND_CONSCRIPT:
        case SIM_COMMAND_TRAIN:
        case SIM_COMMAND_MERCHANT:
        case SIM_COMMAND_PRODUCE:   *city_id = command->city.city_id;     *officer = command->city.officers[0];     break;
        case SIM_COMMAND_DEVELOP:   *city_id = command->develop.city_id;  *officer = command->develop.officers[0];  break;
        case SIM_COMMAND_RESEARCH:  *city_id = command->research.city_id; *officer = command->research.officers[0]; break;
        case SIM_COMMAND_SORTIE:
        case SIM_COMMAND_TRANSPORT: *city_id = command->sortie.city_id;   *officer = command->sortie.officers[0];   break;
        default:
        {
            if (command->kind >= SIM_COMMAND_SEARCH && command->kind <= SIM_COMMAND_FACTION_PLOT)
            {
                *city_id = command->court.city_id;
                *officer = command->court.officer;
            }
        } break;
    }
}

/* The record of the last AI turn of the faction, or 0. */
internal sim_ai_record *
sim_ai_record_of(sim_state *sim, u8 force)
{
    for (u32 index = 0; index < SIM_AI_RECORD_CAPACITY; ++index)
    {
        sim_ai_record *record = sim->ai_records + index;
        if (record->turn && record->force == force) return record;
    }
    return 0;
}

/* An empty record for a new turn of the faction: its old place, else the
   oldest place. */
internal sim_ai_record *
sim_ai_record_start(sim_state *sim, u8 force)
{
    sim_ai_record *record = sim_ai_record_of(sim, force);
    if (!record)
    {
        record = sim->ai_records;
        for (u32 index = 1; index < SIM_AI_RECORD_CAPACITY; ++index)
        {
            if (sim->ai_records[index].turn < record->turn) record = sim->ai_records + index;
        }
    }
    record->force = force;
    record->turn  = sim->turn + 1;
    record->count = 0;
    return record;
}

internal void
sim_ai_record_put(sim_ai_record *record, sim_ai_option option)
{
    if (record && record->count < SIM_AI_OPTION_CAPACITY) record->options[record->count++] = option;
}

internal sim_ai_option
sim_ai_option_of(sim_proposal *proposal, b32 chosen)
{
    sim_ai_option option = {};
    option.goal   = AI_GOAL_CITY_COMMAND;
    option.kind   = (u8)proposal->command.kind;
    option.row    = proposal->row;
    option.unit   = SIM_NO_UNIT;
    option.score  = proposal->score;
    option.chosen = chosen;
    sim_command_place(&proposal->command, &option.city_id, &option.officer);
    return option;
}

/* ---- the goals of an AI unit (M15) ------------------------------------------------- */

/* A goal loses one point of score for each hex to its target, up to this
   distance (assumed, Q-185). */
#define SIM_AI_DISTANCE_SCALE 200

struct sim_ai_choice
{
    b32       found;
    u8        city_id;
    hex_coord target;
    u32       score;
};

internal u32
sim_ai_distance_score(u32 bonus, s32 distance)
{
    return bonus + SIM_AI_DISTANCE_SCALE - Minimum((u32)distance, (u32)SIM_AI_DISTANCE_SCALE);
}

/* The turns that the grain of a unit lasts; U32_MAX if the unit eats
   nothing or the save gives no grain. */
internal u32
sim_unit_grain_turns(sim_state *sim, sim_unit *unit)
{
    u32 eaten = sim->content.rules.unit_grain_soldiers ? unit->soldiers / sim->content.rules.unit_grain_soldiers : 0;
    if (unit->grain_unknown || !eaten) return U32_MAX;
    return unit->grain / eaten;
}

/* The enemy unit nearest to the city within enemy.block_hexes, or SIM_NO_UNIT. */
internal u32
sim_ai_nearest_enemy(sim_state *sim, city_component *city)
{
    hex_coord centre = sim->sites[city->city_id].hex;
    u32 best = SIM_NO_UNIT;
    s32 best_distance = (s32)sim->content.rules.enemy_block_hexes + 1;
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        sim_unit *unit = sim->units + index;
        if (!unit->alive || !sim_at_war(sim, unit->force_id, city->owner_force_id)) continue;
        s32 distance = hex_distance(unit->hex, centre);
        if (distance < best_distance) { best = index; best_distance = distance; }
    }
    return best;
}

internal void
sim_ai_keep(sim_ai_choice *choice, u8 city_id, hex_coord target, u32 score)
{
    if (choice->found && choice->score >= score) return;
    choice->found   = true;
    choice->city_id = city_id;
    choice->target  = target;
    choice->score   = score;
}

/* The best target of each goal of the unit (assumed scores, Q-185):
   - defend: an enemy unit near an own city within ai.defend_hexes;
   - take a free city: a city without an owner;
   - attack a city: a city of a faction at war;
   - resupply: the nearest own city, only if the grain does not last the
     march there and ai.grain_margin_turns more. */
internal void
sim_ai_unit_goals(sim_state *sim, u32 unit_id, sim_ai_choice *choices)
{
    sim_rules *rules = &sim->content.rules;
    sim_unit *unit = sim->units + unit_id;
    for (u32 goal = 0; goal < AI_GOAL_COUNT; ++goal) choices[goal] = {};
    u8 history_target = sim_history_target(sim, unit->force_id);
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        hex_coord hex = sim->sites[city->city_id].hex;
        s32 distance = hex_distance(hex, unit->hex);
        if (city->owner_force_id == unit->force_id)
        {
            sim_ai_keep(choices + AI_GOAL_RESUPPLY, city->city_id, hex, sim_ai_distance_score(rules->ai_resupply_score, distance));
            u32 enemy = (u32)distance <= rules->ai_defend_hexes ? sim_ai_nearest_enemy(sim, city) : SIM_NO_UNIT;
            if (enemy != SIM_NO_UNIT)
            {
                hex_coord at = sim->units[enemy].hex;
                sim_ai_keep(choices + AI_GOAL_DEFEND, city->city_id, at, sim_ai_distance_score(rules->ai_defend_bonus, hex_distance(at, unit->hex)));
            }
        }
        else if (city->owner_force_id == SIM_NO_FORCE)
        {
            sim_ai_keep(choices + AI_GOAL_TAKE_FREE_CITY, city->city_id, hex, sim_ai_distance_score(rules->ai_free_city_bonus, distance));
        }
        else if (sim_at_war(sim, unit->force_id, city->owner_force_id))
        {
            /* E7: the target of a historical event comes before each other city at war. */
            u32 bonus = city->city_id == history_target ? (u32)SIM_AI_DISTANCE_SCALE + 1 : 0;
            if (!bonus && !sim_ai_city_beatable(sim, city, unit->force_id)) continue;
            sim_ai_keep(choices + AI_GOAL_ATTACK_CITY, city->city_id, hex, sim_ai_distance_score(bonus, distance));
        }
    }
    sim_ai_choice *home = choices + AI_GOAL_RESUPPLY;
    if (home->found)
    {
        u32 march      = Maximum(rules->ai_march_hexes, 1u);
        u32 turns_home = ((u32)hex_distance(home->target, unit->hex) + march - 1) / march;
        if (sim_unit_grain_turns(sim, unit) > turns_home + rules->ai_grain_margin_turns) home->found = false;
    }
}

/* Moves the unit one step along the lowest-cost path to the goal, as a
   long order does (sim_unit_step, assumed path rule Q-227). Q-185 gives
   no path rule for the AI. A move in a straight line to the nearest
   reachable hex stops in front of a river or a mountain. An own city at the end
   takes the unit in. Before a city of another owner, the unit stops at
   the nearest hex and attacks from there. */
internal b32
sim_ai_march(sim_state *sim, u32 unit_id, hex_coord goal, sim_event_sink *events)
{
    return sim_unit_step(sim, unit_id, goal, false, 0, events);
}

/* Attacks the hex if it can; returns true after an attack. */
internal b32
sim_ai_attack(sim_state *sim, u32 unit_id, hex_coord hex, sim_event_sink *events)
{
    sim_command attack = sim_make_command(sim, SIM_COMMAND_ATTACK);
    attack.unit.unit = (u16)unit_id;
    attack.unit.to   = hex;
    if (sim_check_attack(sim, &attack.unit).refusal != SIM_REFUSAL_NONE) return false;
    sim_apply(sim, attack, events);
    return true;
}

/* Attacks the first target next to the unit; returns true after an attack. */
internal b32
sim_ai_attack_beside(sim_state *sim, u32 unit_id, sim_event_sink *events)
{
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        if (sim_ai_attack(sim, unit_id, hex_neighbor(sim->units[unit_id].hex, d), events)) return true;
    }
    return false;
}

/* ---- the turn of a computer faction ------------------------------------------------- */

/* The units of the AI: each takes its goal with the best score. A unit
   next to a target attacks it and stays. Else it marches toward its goal
   and attacks after the move. A defender attacks its enemy first. The
   record gets the chosen goal of each unit, then the other goals. */
internal u32
sim_ai_units(sim_state *sim, u8 force, sim_ai_record *record, sim_event_sink *events)
{
    u32 issued = 0;
    sim_ai_option others[SIM_AI_OPTION_CAPACITY];
    u32 other_count = 0;
    for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
    {
        sim_unit *unit = sim->units + unit_id;
        if (!unit->alive || unit->force_id != force || unit->aid_turns || !sim_unit_type(sim, unit->unit_type) || unit->acted_turn == sim->turn + 1) continue;
        sim_ai_choice choices[AI_GOAL_COUNT];
        sim_ai_unit_goals(sim, unit_id, choices);
        u32 best = AI_GOAL_COUNT;
        for (u32 goal = AI_GOAL_DEFEND; goal < AI_GOAL_COUNT; ++goal)
        {
            if (choices[goal].found && (best == AI_GOAL_COUNT || choices[goal].score > choices[best].score)) best = goal;
        }
        for (u32 goal = AI_GOAL_DEFEND; goal < AI_GOAL_COUNT; ++goal)
        {
            if (!choices[goal].found) continue;
            sim_ai_option option = {};
            option.goal    = (u8)goal;
            option.kind    = SIM_COMMAND_MOVE;
            option.city_id = choices[goal].city_id;
            option.row     = 0xFF;
            option.chosen  = goal == best;
            option.unit    = (u16)unit_id;
            option.officer = unit->officers[0];
            option.score   = choices[goal].score;
            option.target  = choices[goal].target;
            if (option.chosen) sim_ai_record_put(record, option);
            else if (other_count < ArrayCount(others)) others[other_count++] = option;
        }
        if (best == AI_GOAL_COUNT) continue;
        /* A unit in contact fights where it stands; only a unit without grain leaves the fight. */
        if (best == AI_GOAL_DEFEND && sim_ai_attack(sim, unit_id, choices[best].target, events)) { issued++; continue; }
        if (best != AI_GOAL_RESUPPLY && sim_ai_attack_beside(sim, unit_id, events)) { issued++; continue; }
        if (unit->moved_turn != sim->turn + 1 && sim_ai_march(sim, unit_id, choices[best].target, events)) issued++;
        if (!unit->alive) continue;
        if (best == AI_GOAL_DEFEND && sim_ai_attack(sim, unit_id, choices[best].target, events)) { issued++; continue; }
        if (sim_ai_attack_beside(sim, unit_id, events)) issued++;
    }
    for (u32 index = 0; index < other_count; ++index) sim_ai_record_put(record, others[index]);
    return issued;
}

/* Runs a plan: the best proposal, then a new plan, up to the budget. The
   record gets each chosen proposal; *first keeps the first plan. */
internal u32
sim_ai_run_plan(sim_state *sim, u8 force, u8 corps_filter, u32 flags, u32 budget, sim_ai_record *record, sim_plan *first,
                sim_event_sink *events)
{
    u32 issued = 0;
    if (first) first->count = 0;
    for (u32 step = 0; step < budget; ++step)
    {
        sim_plan plan;
        sim_plan_faction(sim, force, SIM_NO_CITY, corps_filter, flags, &plan);
        if (!plan.count) break;
        if (first && step == 0) *first = plan;
        sim_ai_record_put(record, sim_ai_option_of(plan.items, true));
        sim_command command = plan.items[0].command;
        command.id = sim->next_command_id++;
        sim_apply(sim, command, events);
        issued++;
    }
    return issued;
}

/* The proposals of the first plan that the AI did not choose, the highest
   score first, after the chosen options. */
internal void
sim_ai_record_unchosen(sim_ai_record *record, sim_plan *first)
{
    u32 chosen_count = record->count;
    for (u32 index = 0; index < first->count; ++index)
    {
        sim_ai_option option = sim_ai_option_of(first->items + index, false);
        b32 done = false;
        for (u32 earlier = 0; earlier < chosen_count && !done; ++earlier)
        {
            sim_ai_option *chosen = record->options + earlier;
            done = chosen->goal == AI_GOAL_CITY_COMMAND && chosen->kind == option.kind && chosen->city_id == option.city_id && chosen->row == option.row;
        }
        if (!done) sim_ai_record_put(record, option);
    }
}

/* The turn of a computer faction: its cities, then its units. */
internal void
sim_run_ai_faction(sim_state *sim, u8 force, u32 cause, sim_event_sink *events)
{
    if (!sim->settings.ai || !sim->has_content) return;
    /* 遊戲等級 (assumed, Q-187): half, the same, 1.5 times or 2 times the commands. */
    static const u32 percent[DIFFICULTY_COUNT] = { 50, 100, 150, 200 };
    u32 budget = Maximum(1u, sim_percent_of(sim->content.rules.ai_commands, percent[Minimum((u32)sim->settings.difficulty, (u32)DIFFICULTY_COUNT - 1)]));
    sim_ai_record *record = sim_ai_record_start(sim, force);
    sim_plan first;
    u32 issued = sim_ai_run_plan(sim, force, 0xFF, CORPS_FLAG_FREE_HAND, budget, record, &first, events);
    issued += sim_ai_units(sim, force, record, events);
    sim_ai_authority(sim, force, events);   /* E6: the crown and the interdict */
    sim_ai_record_unchosen(record, &first);
    if (sim->pending.active && sim->pending.force == force) sim->pending.active = false;   /* the AI takes the first choice */
    sim_event planned = sim_event_make(sim, SIM_EVENT_AI_PLANNED, cause);
    planned.faction = force;
    planned.delta   = (s32)issued;
    planned.assumed = true;
    sim_emit(events, planned);
}

/* The delegated corps of a human faction act at the start of its turn,
   each with its own action points (knowledge/ai.md: the points come from
   max(統率, 魅力) of the leader; assumed amount, Q-185). */
internal void
sim_run_delegated_corps(sim_state *sim, u8 force, u32 cause, sim_event_sink *events)
{
    if (!sim->settings.ai || !sim->has_content) return;
    for (u32 index = 1; index < SIM_CORPS_CAPACITY; ++index)
    {
        sim_corps *corps = sim->corps[force] + index;
        officer_component *viceroy = sim_find_officer(sim, corps->viceroy);
        if (!corps->alive || !viceroy) continue;
        u32 gain = Maximum((u32)viceroy->leadership, (u32)viceroy->charm) / Maximum(sim->content.rules.corps_ap_divisor, 1u);
        sim->corps_ap[force][index] = Minimum(sim->corps_ap[force][index] + gain, sim->content.rules.ap_cap);
        u32 saved = sim->turns.action_points[force];
        sim->turns.action_points[force] = sim->corps_ap[force][index];
        sim_ai_run_plan(sim, force, (u8)index, corps->flags, sim->content.rules.corps_commands, 0, 0, events);
        sim->corps_ap[force][index] = sim->turns.action_points[force];
        sim->turns.action_points[force] = saved;
    }
    (void)cause;
}
