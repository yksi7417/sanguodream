/* The generic events of M8 (events.md, "Generic events"): the 21 events of
   events.tsv, each with a handler of its key. The table holds the dates,
   the chances and the numbers; the handlers hold the conditions and the
   effects. A choice of a human faction waits as sim->pending; a computer
   faction takes the first choice. Included by sim.cpp after bases.cpp.

   The trigger values are documented; the faint values and most chances
   are assumed (Q-184), and the events say so. The historical events of
   the scenarios come with the content packs of M9 (F-131). */

struct sim_event_context
{
    u8  force;
    u8  city;
    u16 officer;                         /* the searcher, the commander */
    u32 unit;
};

/* The officer whose Big5 name (family and given name) is name i of the
   event, or 0. */
internal officer_component *
sim_officer_named(sim_state *sim, event_kind *kind, u32 name)
{
    if (name >= kind->name_count) return 0;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        u8 full[OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE];
        u32 size = 0;
        for (u32 at = 0; at < OFFICER_FAMILY_NAME_SIZE && officer->family_name[at]; ++at) full[size++] = officer->family_name[at];
        for (u32 at = 0; at < OFFICER_GIVEN_NAME_SIZE && officer->given_name[at]; ++at) full[size++] = officer->given_name[at];
        if (size == kind->name_sizes[name] && memcmp(full, kind->names[name], size) == 0) return officer;
    }
    return 0;
}

internal u32
sim_days_since_start(sim_state *sim)
{
    s64 days = calendar_days_between(sim->turns.start_date, sim->turns.date);
    return days > 0 ? (u32)days : 0;
}

internal void
sim_fire_event(sim_state *sim, event_kind *kind, u8 force, officer_component *officer, u32 cause, sim_event_sink *events)
{
    sim->event_turn[kind->id] = sim->turn + 1;
    sim_event event = sim_event_make(sim, SIM_EVENT_EVENT_FIRED, cause);
    event.faction = force;
    event.detail  = kind->id;
    event.subject = officer ? sim_officer_entity(sim, officer->officer_id) : 0;
    event.assumed = kind->chance != 100;
    sim_emit(events, event);
}

/* A choice of a human faction waits; a computer faction takes choice 0. */
internal void sim_resolve_event(sim_state *sim, u8 choice, u32 cause, sim_event_sink *events);

internal void
sim_ask_event(sim_state *sim, event_kind *kind, u8 force, u16 officer_a, u16 officer_b, u8 city, u8 choices, u32 cause, sim_event_sink *events)
{
    sim_pending_event *pending = &sim->pending;
    pending->active    = true;
    pending->event     = kind->id;
    pending->force     = force;
    pending->officer_a = officer_a;
    pending->officer_b = officer_b;
    pending->city      = city;
    pending->choices   = choices;
    pending->cause     = cause;
    pending->officer_c = SIM_NO_OFFICER;
    pending->city_2    = SIM_NO_CITY;
    if (force >= SIM_FORCE_CAPACITY || !sim->turns.human[force]) sim_resolve_event(sim, 0, cause, events);
}

/* G2-027: a transport reached its city. The cargo is in the city and the
   officers stand in it. The question asks where they go: 0 they stay, 1
   they go back to the home city on the road (travel.cpp). A computer
   faction keeps them. */
internal void
sim_transport_arrived(sim_state *sim, const u16 *officers, u8 city, u8 home, u8 force, u32 cause, sim_event_sink *events)
{
    sim_event event = sim_event_make(sim, SIM_EVENT_TRANSPORT_ARRIVED, cause);
    event.subject = sim_find_officer(sim, officers[0]) ? sim_officer_entity(sim, officers[0]) : 0;
    event.faction = force;
    event.detail  = city;
    event.delta   = home;
    sim_emit(events, event);
    u8 row = content_find_event(&sim->content, "transport_arrived");
    if (row >= sim->content.event_count || sim->pending.active || home == SIM_NO_CITY || home == city) return;
    sim_ask_event(sim, sim->content.events + row, force, officers[0], officers[1], city, 2, cause, events);
    if (sim->pending.active)
    {
        sim->pending.officer_c = officers[2];
        sim->pending.city_2    = home;
    }
}

internal b32
sim_event_roll(sim_state *sim, event_kind *kind, b32 forced)
{
    return forced || kind->chance >= 100 || sim_random_below(sim, 100) < kind->chance;
}

/* The ruler of the faction when it stands in a city, or 0. */
internal officer_component *
sim_free_ruler(sim_state *sim, u8 force)
{
    officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, force));
    return ruler && ruler->current_position < SAVE_CITY_COUNT ? ruler : 0;
}

/* ---- the handlers of the faction events -------------------------------------------- */

/* 統一前謀反的嫌疑: p1 rank, p2 corps, p3 loyalty that all lose, p4 public
   order that all cities lose, p5 loyalty that the informer loses. */
internal b32
sim_event_treason(sim_state *sim, event_kind *kind, sim_event_context *ctx, b32 forced, u32 cause, sim_event_sink *events)
{
    u8 force = ctx->force;
    officer_component *ruler = sim_free_ruler(sim, force);
    if (!ruler) return false;
    u32 corps_count = 0;
    officer_component *viceroy = 0;
    for (u32 index = 1; index < SIM_CORPS_CAPACITY; ++index)
    {
        sim_corps *corps = sim->corps[force] + index;
        officer_component *candidate = corps->alive ? sim_find_officer(sim, corps->viceroy) : 0;
        if (!candidate || candidate->force_id != force || !sim_officer_is_active(candidate)) continue;   /* a stale corps record names no 都督 */
        corps_count++;
        if (!sim_officers_bonded(candidate, ruler) && !sim_officers_blood(candidate, ruler) && candidate->loyalty < 100) viceroy = candidate;
    }
    if (!forced && (sim->rank[force] < kind->p[0] || corps_count < kind->p[1])) return false;
    if (!viceroy) return false;
    officer_component *informer = 0;
    for (u32 index = 0; index < sim->officers.count && !informer; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (sim_officer_is_active(officer) && officer->force_id == force && officer != viceroy && officer != ruler && officer->justice <= 1 &&
            !sim_officers_blood(officer, viceroy)) informer = officer;
    }
    if (!informer && !forced) return false;
    if (!sim_event_roll(sim, kind, forced)) return false;
    sim_fire_event(sim, kind, force, viceroy, cause, events);
    sim_ask_event(sim, kind, force, informer ? informer->officer_id : (u16)SIM_NO_OFFICER, viceroy->officer_id, SIM_NO_CITY, 2, cause, events);
    return true;
}

/* 銅雀台: p1 cities, p2 rank, p3 the city (鄴). */
internal b32
sim_event_bronze_bird(sim_state *sim, event_kind *kind, sim_event_context *ctx, b32 forced, u32 cause, sim_event_sink *events)
{
    officer_component *ruler = sim_free_ruler(sim, ctx->force);
    sim_item *bronze = 0;
    for (u32 index = 0; index < SAVE_ITEM_COUNT && !bronze; ++index)
    {
        sim_item *item = sim->items + index;
        if (item->exists && item->type == ITEM_BRONZE && item->owner == SIM_NO_OFFICER && item->city == SIM_NO_CITY) bronze = item;
    }
    if (!ruler || !bronze) return false;
    if (!forced && (ruler->current_position != kind->p[2] || sim_faction_city_count(sim, ctx->force) <= kind->p[0] || sim->rank[ctx->force] < kind->p[1])) return false;
    if (!sim_event_roll(sim, kind, forced)) return false;
    bronze->owner = ruler->officer_id;
    sim_fire_event(sim, kind, ctx->force, ruler, cause, events);
    return true;
}

/* 推舉－文官 and 推舉－武官: a loyal subordinate in the city of the ruler
   finds a free officer linked to him; a debate or a duel decides (the
   computer rule: the higher ability wins more often, assumed). */
internal b32
sim_event_recommend(sim_state *sim, event_kind *kind, sim_event_context *ctx, b32 forced, b32 military, u32 cause, sim_event_sink *events)
{
    officer_component *ruler = sim_free_ruler(sim, ctx->force);
    if (!ruler) return false;
    u32 merit_gain = military ? kind->p[3] : kind->p[2];
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *subordinate = sim->officers.data + index;
        if (!sim_officer_is_active(subordinate) || subordinate->force_id != ctx->force || subordinate == ruler ||
            subordinate->current_position != ruler->current_position || subordinate->loyalty < kind->p[0]) continue;
        if ((military ? subordinate->war : subordinate->politics) < kind->p[1]) continue;
        for (u32 other = 0; other < sim->officers.count; ++other)
        {
            officer_component *candidate = sim->officers.data + other;
            if (candidate->status != OFFICER_STATUS_FREE_OR_NOT_APPEARED || candidate->appearance_year > sim->turns.date.year) continue;
            b32 linked = sim_officers_bonded(subordinate, candidate) || sim_officers_blood(subordinate, candidate) ||
                         sim_officer_likes(subordinate, candidate->officer_id) || sim_officer_likes(candidate, subordinate->officer_id);
            if (!linked) continue;
            if (military ? candidate->war < kind->p[2] : (candidate->war >= candidate->intelligence && candidate->war >= candidate->politics)) continue;
            if (sim_officer_dislikes(candidate, ruler->officer_id)) continue;
            if (!sim_event_roll(sim, kind, forced)) return false;
            sim_fire_event(sim, kind, ctx->force, candidate, cause, events);
            s32 edge = military ? (s32)subordinate->war - (s32)candidate->war : (s32)subordinate->intelligence - (s32)candidate->intelligence;
            if ((s32)sim_random_below(sim, 100) < 50 + edge)
            {
                sim_join_faction(sim, candidate, ctx->force, (u8)ruler->current_position, cause, events);
                candidate->loyalty = subordinate->loyalty;
                officer_component *pair[2] = { subordinate, candidate };
                for (u32 side = 0; side < 2; ++side)
                {
                    u32 merit = Minimum((u32)pair[side]->merit + merit_gain, 0xFFFFu);
                    sim_emit_officer(sim, SIM_EVENT_MERIT_GAINED, pair[side], (s32)(merit - pair[side]->merit), 0, false, cause, events);
                    pair[side]->merit = (u16)merit;
                }
            }
            return true;
        }
    }
    return false;
}

/* 五虎將軍: p1 rank, p2 cities, p3 force, p4 merit, p5 the merit that each gains. */
internal b32
sim_event_five_tigers(sim_state *sim, event_kind *kind, sim_event_context *ctx, b32 forced, u32 cause, sim_event_sink *events)
{
    u8 force = ctx->force;
    if (!forced && (sim->rank[force] < kind->p[0] || sim_faction_city_count(sim, force) <= kind->p[1] ||
                    !sim_find_officer(sim, sim->adviser[force]))) return false;
    officer_component *tigers[5] = {};
    u32 count = 0;
    for (u32 index = 0; index < sim->officers.count && count < 5; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!sim_officer_is_active(officer) || officer->force_id != force || officer->status == OFFICER_STATUS_RULER) continue;
        if (!forced && (officer->war <= kind->p[2] || officer->merit <= kind->p[3] || officer->loyalty < 100)) continue;
        tigers[count++] = officer;
    }
    if (count < 5 || !sim_event_roll(sim, kind, forced)) return false;
    sim_fire_event(sim, kind, force, tigers[0], cause, events);
    for (u32 index = 0; index < 5; ++index)
    {
        u32 merit = Minimum((u32)tigers[index]->merit + kind->p[4], 0xFFFFu);
        sim_emit_officer(sim, SIM_EVENT_MERIT_GAINED, tigers[index], (s32)(merit - tigers[index]->merit), 0, true, cause, events);
        tigers[index]->merit = (u16)merit;
        sim_gain_experience(sim, tigers[index], CONTENT_ABILITY_FORCE, sim->content.rules.event_experience, cause, events);
        sim_gain_experience(sim, tigers[index], CONTENT_ABILITY_LEADERSHIP, sim->content.rules.event_experience, cause, events);
    }
    return true;
}

/* 少帝即位, 獻帝即位, 獻帝駕崩: p1 year, p2 month. */
internal b32
sim_event_emperor(sim_state *sim, event_kind *kind, u8 before, u8 after, b32 forced, u32 cause, sim_event_sink *events)
{
    if (sim->emperor.who != before) return false;
    u32 stamp = (u32)sim->turns.date.year * 12 + sim->turns.date.month;
    if (!forced && stamp < kind->p[0] * 12 + kind->p[1]) return false;
    if (!sim_event_roll(sim, kind, forced)) return false;
    sim_fire_event(sim, kind, SIM_NO_FORCE, 0, cause, events);
    sim_set_emperor(sim, after, cause, events);
    return true;
}

/* ---- the travellers: after 探索人材 in their city ------------------------------------------ */

internal b32
sim_event_traveller(sim_state *sim, event_kind *kind, sim_event_context *ctx, b32 forced, u32 cause, sim_event_sink *events)
{
    officer_component *traveller = sim_officer_named(sim, kind, 0);
    officer_component *searcher  = sim_find_officer(sim, ctx->officer);
    city_component *city = sim_find_city(sim, ctx->city);
    if (!traveller || !searcher || !city || traveller->status != OFFICER_STATUS_FREE_OR_NOT_APPEARED) return false;
    if (!forced && traveller->base_location != ctx->city) return false;
    b32 disaster = false;
    for (u32 index = 0; index < CONTENT_DISASTER_CAPACITY; ++index) disaster |= city->disaster_turns[index] != 0;
    u32 experience = sim->content.rules.event_experience;
    const char *key = kind->key;
    if (strcmp(key, "visit_yuji") == 0)
    {
        b32 holds = city->gold >= kind->p[0] && city->public_order >= kind->p[1] && city->troops + kind->p[2] <= sim_city_soldier_cap(sim, city->city_id) &&
                    sim->technique_points[ctx->force] <= kind->p[3];
        if (!forced && !holds) return false;
        sim_fire_event(sim, kind, ctx->force, traveller, cause, events);
        sim_ask_event(sim, kind, ctx->force, searcher->officer_id, traveller->officer_id, ctx->city, 3, cause, events);
        return true;
    }
    if (!forced && (disaster || city->public_order < kind->p[0])) return false;
    if (strcmp(key, "visit_guanlu") == 0 && !forced)
    {
        b32 liked = false, disliked = false;
        for (u32 slot = 0; slot < OFFICER_RELATION_SLOTS; ++slot)
        {
            liked    |= searcher->liked[slot] != SIM_NO_OFFICER;
            disliked |= searcher->disliked[slot] != SIM_NO_OFFICER;
        }
        if (!liked || !disliked) return false;
    }
    if (strcmp(key, "visit_simahui_a") == 0 && !forced)
    {
        b32 pupil = false;
        for (u32 name = 1; name < kind->name_count; ++name) pupil |= sim_officer_named(sim, kind, name) == searcher;
        if (!pupil) return false;
    }
    if (strcmp(key, "visit_huangchengyan") == 0 && !forced && searcher->merit < kind->p[1]) return false;
    if (strcmp(key, "visit_miheng") == 0)
    {
        u32 points = sim->technique_points[ctx->force];
        if (!forced && (searcher->intelligence < kind->p[0] || points < 1 || points > kind->p[1])) return false;
        sim_fire_event(sim, kind, ctx->force, traveller, cause, events);
        s32 edge = (s32)searcher->intelligence - (s32)traveller->intelligence;
        b32 won = (s32)sim_random_below(sim, 100) < 50 + edge;   /* the debate by the computer rule (assumed) */
        sim_add_technique_points(sim, ctx->force, won ? (s32)kind->p[2] : -(s32)kind->p[3], cause, events);
        sim_gain_experience(sim, searcher, CONTENT_ABILITY_POLITICS, experience, cause, events);
        return true;
    }
    sim_fire_event(sim, kind, ctx->force, traveller, cause, events);
    if (strcmp(key, "visit_huatuo") == 0)
    {
        for (u32 index = 0; index < sim->officers.count; ++index)
        {
            officer_component *officer = sim->officers.data + index;
            if (officer->current_position != ctx->city || officer->health == HEALTH_GOOD) continue;
            officer->health = HEALTH_GOOD;
            officer->health_turns = 0;
            sim_emit_officer(sim, SIM_EVENT_HEALTH_CHANGED, officer, HEALTH_GOOD, 0, false, cause, events);
        }
    }
    u32 ability = CONTENT_ABILITY_POLITICS;
    if (strcmp(key, "visit_simahui_a") == 0 || strcmp(key, "visit_simahui_b") == 0) ability = CONTENT_ABILITY_INTELLECT;
    sim_gain_experience(sim, searcher, ability, experience, cause, events);
    return true;
}

/* ---- after 待命: ruins, temples, spouses, sworn brothers ------------------------------------- */

internal u32
sim_unit_best_charm(sim_state *sim, sim_unit *unit)
{
    u32 best = 0;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, unit->officers[place]);
        if (officer && officer->charm > best) best = officer->charm;
    }
    return best;
}

/* The ability of the month for a ruin (events.md: 1 cavalry, 2 leadership,
   3 crossbow, 4 force, 5 siege, 6 charm, 7 navy, 8 intellect, 9 pike, 10
   politics, 11 halberd, 12 charm). An aptitude month raises the aptitude
   by one grade (assumed). */
internal void
sim_ruin_reward(sim_state *sim, officer_component *officer, u32 month, u32 amount, u32 cause, sim_event_sink *events)
{
    static const s8 abilities[13] = { -1, -1, CONTENT_ABILITY_LEADERSHIP, -1, CONTENT_ABILITY_FORCE, -1, CONTENT_ABILITY_CHARM, -1,
                                      CONTENT_ABILITY_INTELLECT, -1, CONTENT_ABILITY_POLITICS, -1, CONTENT_ABILITY_CHARM };
    static const s8 aptitudes[13] = { -1, OFFICER_APTITUDE_CAVALRY, -1, OFFICER_APTITUDE_CROSSBOW, -1, OFFICER_APTITUDE_SIEGE, -1,
                                      OFFICER_APTITUDE_NAVY, -1, OFFICER_APTITUDE_SPEAR, -1, OFFICER_APTITUDE_HALBERD, -1 };
    if (month > 12) return;
    if (abilities[month] >= 0) sim_gain_experience(sim, officer, (u32)abilities[month], amount, cause, events);
    else if (aptitudes[month] >= 0 && officer->unit_aptitudes[aptitudes[month]] < 3) officer->unit_aptitudes[aptitudes[month]]++;
}

/* 發現遺跡 (p1 merit, p2 charm, p3 experience) and 發現廟宇 (p1 merit, p2
   charm): a unit that waits next to the site. A temple gives a skill (the
   rare 神算 and 妖術 are left out, assumed). */
internal b32
sim_event_relic(sim_state *sim, event_kind *kind, sim_event_context *ctx, b32 temple, b32 forced, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim_find_unit(sim, ctx->unit);
    officer_component *commander = unit ? sim_find_officer(sim, unit->officers[0]) : 0;
    if (!commander) return false;
    b32 beside = false;
    for (u32 index = 0; index < SIM_RELIC_COUNT; ++index)
    {
        sim_relic *relic = sim->relics + index;
        if (relic->alive && relic->kind == (temple ? RELIC_TEMPLE : RELIC_RUIN) && hex_distance(relic->hex, unit->hex) == 1) beside = true;
    }
    if (!forced && (!beside || commander->merit < kind->p[0] || sim_unit_best_charm(sim, unit) < kind->p[1])) return false;
    if (!sim_event_roll(sim, kind, forced)) return false;
    sim_fire_event(sim, kind, unit->force_id, commander, cause, events);
    if (!temple)
    {
        sim_ruin_reward(sim, commander, sim->turns.date.month, kind->p[2], cause, events);
        return true;
    }
    u32 hooked[CONTENT_SKILL_COUNT], count = 0;
    for (u32 index = 0; index < sim->content.skill_count; ++index)
    {
        skill_kind *skill = sim->content.skills + index;
        if (skill->hooked && skill->number != 64 && skill->number != 71 && skill->number != 72) hooked[count++] = skill->number;
    }
    if (count) commander->skill_number = (u8)(hooked[sim_random_below(sim, count)] - 1);
    return true;
}

/* 夫婦的助力 and 義兄弟之氣炎 (p1 morale): a unit that waits with a spouse
   or a sworn brother in it or in an adjacent own unit. */
internal b32
sim_event_bond(sim_state *sim, event_kind *kind, sim_event_context *ctx, b32 spouse, b32 forced, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim_find_unit(sim, ctx->unit);
    if (!unit) return false;
    terrain_kind *terrain = sim_terrain_at(sim, unit->hex);
    if (!forced && terrain && (strcmp(terrain->key, "poison_spring") == 0 || strcmp(terrain->key, "plank_road") == 0)) return false;
    u32 neighbours[HEX_DIRECTION_COUNT + 1], count = 0;
    neighbours[count++] = ctx->unit;
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        u32 other = sim_unit_at(sim, hex_neighbor(unit->hex, d));
        if (other != SIM_NO_UNIT && sim->units[other].force_id == unit->force_id) neighbours[count++] = other;
    }
    officer_component *a = 0, *b = 0;
    for (u32 i = 0; i < count && !a; ++i)
    {
        for (u32 j = 0; j < count && !a; ++j)
        {
            for (u32 p = 0; p < SIM_COMMAND_OFFICERS && !a; ++p)
            {
                for (u32 q = 0; q < SIM_COMMAND_OFFICERS && !a; ++q)
                {
                    officer_component *x = sim_find_officer(sim, sim->units[neighbours[i]].officers[p]);
                    officer_component *y = sim_find_officer(sim, sim->units[neighbours[j]].officers[q]);
                    if (!x || !y || x == y) continue;
                    b32 bond = spouse ? x->spouse == y->officer_id :
                               (x->sworn_brother_group_head != SIM_NO_OFFICER && x->sworn_brother_group_head == y->sworn_brother_group_head);
                    if (bond) { a = x; b = y; }
                }
            }
        }
    }
    if (!a) return false;
    if (spouse && !forced && a->health == HEALTH_GOOD && b->health == HEALTH_GOOD) return false;
    if (!spouse && !forced && unit->morale > kind->p[0]) return false;
    if (!sim_event_roll(sim, kind, forced)) return false;
    sim_fire_event(sim, kind, unit->force_id, a, cause, events);
    u32 experience = sim->content.rules.event_experience;
    officer_component *pair[2] = { a, b };
    for (u32 side = 0; side < 2; ++side)
    {
        if (spouse)
        {
            pair[side]->health = HEALTH_GOOD;
            pair[side]->health_turns = 0;
            pair[side]->stamina = 100;
        }
        sim_gain_experience(sim, pair[side], CONTENT_ABILITY_CHARM, experience, cause, events);
    }
    if (!spouse) sim_change_morale(sim, ctx->unit, (s32)sim_city_morale_max(sim, sim_find_city(sim, unit->home_city) ? sim_find_city(sim, unit->home_city) : sim->cities.data),
                                   cause, events);
    return true;
}

/* ---- the dispatch ------------------------------------------------------------------------ */

internal b32
sim_event_handle(sim_state *sim, event_kind *kind, sim_event_context *ctx, b32 forced, u32 cause, sim_event_sink *events)
{
    const char *key = kind->key;
    if (strcmp(key, "treason") == 0)            return sim_event_treason(sim, kind, ctx, forced, cause, events);
    if (strcmp(key, "bronze_bird") == 0)        return sim_event_bronze_bird(sim, kind, ctx, forced, cause, events);
    if (strcmp(key, "recommend_civil") == 0)    return sim_event_recommend(sim, kind, ctx, forced, false, cause, events);
    if (strcmp(key, "recommend_military") == 0) return sim_event_recommend(sim, kind, ctx, forced, true, cause, events);
    if (strcmp(key, "five_tigers") == 0)        return sim_event_five_tigers(sim, kind, ctx, forced, cause, events);
    if (strcmp(key, "ruins") == 0)              return sim_event_relic(sim, kind, ctx, false, forced, cause, events);
    if (strcmp(key, "temple") == 0)             return sim_event_relic(sim, kind, ctx, true, forced, cause, events);
    if (strcmp(key, "shao_accession") == 0)     return sim_event_emperor(sim, kind, EMPEROR_LING, EMPEROR_SHAO, forced, cause, events);
    if (strcmp(key, "xian_accession") == 0)     return sim_event_emperor(sim, kind, EMPEROR_SHAO, EMPEROR_XIAN, forced, cause, events);
    if (strcmp(key, "xian_death") == 0)         return sim_event_emperor(sim, kind, EMPEROR_XIAN, EMPEROR_NONE, forced, cause, events);
    if (strncmp(key, "visit_", 6) == 0)         return sim_event_traveller(sim, kind, ctx, forced, cause, events);
    if (strcmp(key, "spouse_help") == 0)        return sim_event_bond(sim, kind, ctx, true, forced, cause, events);
    if (strcmp(key, "sworn_spirit") == 0)       return sim_event_bond(sim, kind, ctx, false, forced, cause, events);
    return false;   /* 破壞遺跡、廟宇: no attack on a site yet */
}

/* The checks of the calendar: day 1 of each month (and the quarter
   months), for each faction with a ruler. The travellers move first. */
internal void
sim_run_events_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    if (!sim->settings.events || !sim->has_content) return;
    u32 days = sim_days_since_start(sim);
    u32 month = sim->turns.date.month;
    for (u32 row = 0; row < sim->content.event_count; ++row)
    {
        event_kind *kind = sim->content.events + row;
        officer_component *traveller = kind->check == EVENT_CHECK_SEARCH ? sim_officer_named(sim, kind, 0) : 0;
        if (traveller && traveller->status == OFFICER_STATUS_FREE_OR_NOT_APPEARED)
        {
            traveller->base_location = traveller->current_position = (u16)sim_random_below(sim, SAVE_CITY_COUNT);   /* a traveller moves each month */
        }
    }
    for (u32 row = 0; row < sim->content.event_count && !sim->pending.active; ++row)
    {
        event_kind *kind = sim->content.events + row;
        b32 due = kind->check == EVENT_CHECK_MONTH || (kind->check == EVENT_CHECK_QUARTER && ((kind->months >> month) & 1));
        if (!due || days < kind->min_days) continue;
        if (strncmp(kind->key, "shao", 4) == 0 || strncmp(kind->key, "xian", 4) == 0)
        {
            sim_event_context ctx = {};
            sim_event_handle(sim, kind, &ctx, false, cause, events);
            continue;
        }
        for (u32 force = 0; force < SIM_FORCE_CAPACITY && !sim->pending.active; ++force)
        {
            if (sim_force_ruler(sim, (u8)force) == SIM_NO_OFFICER) continue;
            sim_event_context ctx = {};
            ctx.force = (u8)force;
            ctx.city  = SIM_NO_CITY;
            if (sim_event_handle(sim, kind, &ctx, false, cause, events)) break;
        }
    }
}

/* After 探索人材 by the officer in the city. */
internal void
sim_events_after_search(sim_state *sim, u8 city_id, u16 officer, u32 cause, sim_event_sink *events)
{
    city_component *city = sim_find_city(sim, city_id);
    if (!sim->settings.events || !city || sim->pending.active) return;
    for (u32 row = 0; row < sim->content.event_count; ++row)
    {
        event_kind *kind = sim->content.events + row;
        if (kind->check != EVENT_CHECK_SEARCH) continue;
        sim_event_context ctx = {};
        ctx.force   = city->owner_force_id;
        ctx.city    = city_id;
        ctx.officer = officer;
        if (sim_event_handle(sim, kind, &ctx, false, cause, events)) return;
    }
}

/* After 待命 of the unit. */
internal void
sim_events_after_wait(sim_state *sim, u32 unit_id, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim_find_unit(sim, unit_id);
    if (!sim->settings.events || !unit || sim->pending.active) return;
    for (u32 row = 0; row < sim->content.event_count; ++row)
    {
        event_kind *kind = sim->content.events + row;
        if (kind->check != EVENT_CHECK_WAIT) continue;
        sim_event_context ctx = {};
        ctx.force = unit->force_id;
        ctx.unit  = unit_id;
        if (sim_event_handle(sim, kind, &ctx, false, cause, events)) return;
    }
}

/* ---- the choice and the editor ------------------------------------------------------------- */

/* The effect of the choice of a waiting event. 統一前謀反的嫌疑: 0 punish the
   informer, 1 punish the 都督 (he rebels with his corps). 于吉: 0 give gold,
   1 refuse, 2 kill him. */
internal void
sim_resolve_event(sim_state *sim, u8 choice, u32 cause, sim_event_sink *events)
{
    sim_pending_event pending = sim->pending;
    sim->pending.active = false;
    event_kind *kind = sim->content.events + pending.event;
    sim_event chosen = sim_event_make(sim, SIM_EVENT_EVENT_CHOSEN, cause);
    chosen.faction = pending.force;
    chosen.detail  = pending.event;
    chosen.delta   = choice;
    sim_emit(events, chosen);
    officer_component *a = sim_find_officer(sim, pending.officer_a), *b = sim_find_officer(sim, pending.officer_b);
    u32 experience = sim->content.rules.event_experience;
    if (strcmp(kind->key, "transport_arrived") == 0)
    {
        /* G2-027: choice 1 sends the officers back to the home city. */
        if (choice != 1 || pending.city_2 == SIM_NO_CITY) return;
        u16 ids[SIM_COMMAND_OFFICERS] = { pending.officer_a, pending.officer_b, pending.officer_c };
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
        {
            officer_component *officer = sim_find_officer(sim, ids[place]);
            if (!officer || officer->current_position != pending.city || officer->travel_turns) continue;
            u32 days = sim_travel_days(sim, pending.city, pending.city_2);
            officer->travel_city  = pending.city_2;
            officer->travel_turns = (u8)Maximum(days / CALENDAR_DAYS_PER_TURN, 1u);
            sim_emit_officer(sim, SIM_EVENT_OFFICER_MOVED, officer, (s32)days, pending.city_2, true, cause, events);
        }
        return;
    }
    if (strcmp(kind->key, "treason") == 0)
    {
        if (choice == 0)
        {
            if (a) sim_change_loyalty(sim, a, -(s32)kind->p[4], cause, events);
            return;
        }
        /* The 都督 founds a faction with the cities of his corps. */
        u8 force = pending.force, fresh = sim_free_force_id(sim);
        if (!b || fresh == SIM_NO_FORCE) return;
        u8 corps = 0;
        for (u32 index = 1; index < SIM_CORPS_CAPACITY; ++index) if (sim->corps[force][index].alive && sim->corps[force][index].viceroy == b->officer_id) corps = (u8)index;
        for (u32 index = 0; index < sim->cities.count; ++index)
        {
            city_component *city = sim->cities.data + index;
            if (city->owner_force_id == force && corps && city->corps == corps)
            {
                city->owner_force_id = fresh;
                city->corps = 0;
            }
        }
        if (corps) sim->corps[force][corps] = {};
        b->force_id = fresh;
        b->status   = OFFICER_STATUS_RULER;
        b->loyalty  = 100;
        sim->rank[fresh] = sim_rank_for(sim, fresh);
        sim_event founded = sim_event_make(sim, SIM_EVENT_FACTION_FOUNDED, cause);
        founded.faction = fresh;
        founded.detail  = force;
        sim_emit(events, founded);
        for (u32 index = 0; index < sim->officers.count; ++index)
        {
            officer_component *officer = sim->officers.data + index;
            if (sim_officer_is_active(officer) && officer->force_id == force && officer->status != OFFICER_STATUS_RULER)
            {
                sim_change_loyalty(sim, officer, -(s32)kind->p[2], cause, events);
            }
        }
        for (u32 index = 0; index < sim->cities.count; ++index)
        {
            city_component *city = sim->cities.data + index;
            if (city->owner_force_id != force) continue;
            u32 lost = Minimum((u32)city->public_order, kind->p[3]);
            city->public_order = (u8)(city->public_order - lost);
            if (lost) sim_emit_city(sim, SIM_EVENT_ORDER_DROPPED, city, -(s32)lost, 0, false, cause, events);
        }
        return;
    }
    if (strcmp(kind->key, "visit_yuji") == 0)
    {
        city_component *city = sim_find_city(sim, pending.city);
        if (!city || !a) return;
        if (choice == 0 && city->gold >= kind->p[4])
        {
            city->gold -= kind->p[4];
            sim_emit_city(sim, SIM_EVENT_GOLD_SPENT, city, -(s32)kind->p[4], 0, false, cause, events);
            sim_add_technique_points(sim, pending.force, (s32)kind->p[5], cause, events);
        }
        else if (choice == 1)
        {
            city->troops = Minimum(city->troops + 1000, sim_city_soldier_cap(sim, city->city_id));   /* the amount is assumed (Q-184) */
        }
        else if (choice == 2)
        {
            u32 lost = Minimum((u32)city->public_order, 20u);
            city->public_order = (u8)(city->public_order - lost);
            if (lost) sim_emit_city(sim, SIM_EVENT_ORDER_DROPPED, city, -(s32)lost, 0, false, cause, events);
            if (a->health < HEALTH_NEAR_DEATH) { a->health++; a->health_turns = (u8)sim->content.rules.wound_turns; }
            if (b) b->status = OFFICER_STATUS_DEAD;
        }
        sim_gain_experience(sim, a, CONTENT_ABILITY_CHARM, experience, cause, events);
    }
}

internal sim_check
sim_check_event_choice(sim_state *sim, sim_m8_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (!sim->pending.active || order->value >= sim->pending.choices) return sim_refuse(check, SIM_REFUSAL_NO_EVENT);
    return check;
}

internal void
sim_event_choice_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    if (sim_check_event_choice(sim, &command.m8).refusal != SIM_REFUSAL_NONE) return;
    sim_resolve_event(sim, command.m8.value, command.id, events);
}

/* SIM_COMMAND_TRIGGER_EVENT: the event fires now for the faction, the city,
   the officer or the unit of the order, without its date and chance. */
internal sim_check
sim_check_trigger_event(sim_state *sim, sim_m8_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (!sim->has_content || order->value >= sim->content.event_count) return sim_refuse(check, SIM_REFUSAL_NO_EVENT);
    if (sim->pending.active || sim_busy(sim)) return sim_refuse(check, SIM_REFUSAL_BUSY);
    return check;
}

internal void
sim_trigger_event_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    if (sim_check_trigger_event(sim, &command.m8).refusal != SIM_REFUSAL_NONE) return;
    sim_event_context ctx = {};
    ctx.force   = command.m8.force;
    ctx.city    = command.m8.city_id;
    ctx.officer = command.m8.officer;
    ctx.unit    = command.m8.officer;   /* the wait events read the unit from the officer field */
    sim_event_handle(sim, sim->content.events + command.m8.value, &ctx, true, command.id, events);
}

/* M10 (F-150): at the start the game picks 6 event sites (map byte 10)
   for one kind and 6 for another kind at random. The engine makes the
   first 6 temples and the next 6 ruins (assumed; Q-193 asks which is
   which). */
internal void
sim_init_relics_on_map(sim_state *sim)
{
    hex_coord sites[64];
    u32 count = 0;
    sim_map_layers *layers = &sim->map_layers;
    for (u32 x = 0; x < sim->map.width; ++x)
    {
        for (u32 y = 0; y < sim->map.height && count < ArrayCount(sites); ++y)
        {
            u32 cell = y * sim->map.width + x;
            if (layers->flags[cell] & SIM_MAP_FLAG_EVENT) sites[count++] = hex_map_coord(&sim->map, cell);
        }
    }
    u32 wanted = 2 * SIM_RELIC_SITES_OF_A_KIND < SIM_RELIC_COUNT ? 2 * SIM_RELIC_SITES_OF_A_KIND : SIM_RELIC_COUNT;
    for (u32 placed = 0; placed < wanted && placed < count; ++placed)
    {
        u32 pick = placed + sim_random_below(sim, count - placed);   /* a partial shuffle: each site once */
        hex_coord chosen = sites[pick];
        sites[pick]   = sites[placed];
        sites[placed] = chosen;
        sim_relic *relic = sim->relics + placed;
        relic->alive = true;
        relic->kind  = (u8)(placed < SIM_RELIC_SITES_OF_A_KIND ? RELIC_TEMPLE : RELIC_RUIN);
        relic->hex   = chosen;
    }
}

/* PLACEHOLDER: on the placeholder map the ruin and the six temples stand
   on free hexes that a hash of the map picks, far from the cities. */
internal void
sim_init_relics(sim_state *sim)
{
    if (sim->map_layers.loaded)
    {
        sim_init_relics_on_map(sim);
        return;
    }
    u32 placed = 0;
    u32 cells = (u32)sim->map.width * sim->map.height;
    for (u32 step = 0; step < cells && placed < SIM_RELIC_PLACEHOLDER_COUNT; ++step)
    {
        u32 cell = (step * 2654435761u + 12345u) % (cells ? cells : 1);
        hex_coord hex = hex_map_coord(&sim->map, cell);
        if (!sim_base_hex_free(sim, hex)) continue;
        b32 apart = true;
        for (u32 city = 0; city < SAVE_CITY_COUNT && apart; ++city) apart = hex_distance(sim->sites[city].hex, hex) > 3;
        for (u32 index = 0; index < placed && apart; ++index) apart = hex_distance(sim->relics[index].hex, hex) > 4;
        if (!apart) continue;
        sim_relic *relic = sim->relics + placed;
        relic->alive = true;
        relic->kind  = (u8)(placed == 0 ? RELIC_RUIN : RELIC_TEMPLE);
        relic->hex   = hex;
        placed++;
    }
}
