/* Officers of M7 (officers-and-relations.md, factions-corps-and-posts.md,
   city-commands.md): the character and the relations of an officer,
   loyalty, experience, growth with age, health, posts and ranks, items, and
   the personnel and ruler commands 探索人材, 登庸武將, 褒賞, 授與, 官職,
   仲介 and 流放. Included by sim.cpp after field.cpp.

   The spec gives the inputs of each rule and the costs of the commands. It
   gives no formula: the chances and amounts are assumed numbers of
   rules.tsv (Q-028, Q-029, Q-177 to Q-179), and their events say so. */

/* ---- the character of an officer ------------------------------------------ */

/* ASSUMED (Q-177): the save fields of the character are not decoded. A
   strong officer with a low intellect is reckless, a strong one bold, a
   wise one calm, the others careful. */
internal u8
sim_assumed_personality(officer_component *officer)
{
    if (officer->war >= 85 && officer->intelligence < 60) return PERSONALITY_RECKLESS;
    if (officer->war >= 70 && officer->war > officer->intelligence) return PERSONALITY_BOLD;
    if (officer->intelligence >= 75) return PERSONALITY_CALM;
    return PERSONALITY_CAREFUL;
}

/* The M7 fields of an officer after a save loads: the relations of the
   record, and assumed values for the fields that are not decoded. */
internal void
sim_init_officer_state(sim_state *sim, officer_component *officer, officer_record *record)
{
    sim_rules *rules = &sim->content.rules;
    officer->loyalty      = officer->status == OFFICER_STATUS_RULER ? 100 : (u8)Minimum(rules->loyalty_start, 100u);
    officer->ambition     = 2;
    officer->justice      = 2;
    officer->personality  = sim_assumed_personality(officer);
    officer->growth       = GROWTH_NORMAL;
    officer->health       = HEALTH_GOOD;
    officer->stamina      = 100;
    officer->post         = CONTENT_NO_POST;
    officer->former_force = SIM_NO_FORCE;
    officer->han_respect  = 1;   /* M8: normal; the save field is not decoded (Q-186) */
    for (u32 slot = 0; slot < OFFICER_RELATION_SLOTS; ++slot)
    {
        officer->liked[slot]    = record ? record->liked_officers[slot] : SIM_NO_OFFICER;
        officer->disliked[slot] = record ? record->disliked_officers[slot] : SIM_NO_OFFICER;
    }
}

internal u32
sim_compatibility_distance(u8 a, u8 b)
{
    u32 distance = a > b ? (u32)(a - b) : (u32)(b - a);
    distance %= 150;
    return distance > 75 ? 150 - distance : distance;
}

internal b32
sim_officer_likes(officer_component *officer, u16 other)
{
    for (u32 slot = 0; officer && slot < OFFICER_RELATION_SLOTS; ++slot)
    {
        if (officer->liked[slot] == other && other != SIM_NO_OFFICER) return true;
    }
    return false;
}

internal b32
sim_officer_dislikes(officer_component *officer, u16 other)
{
    for (u32 slot = 0; officer && slot < OFFICER_RELATION_SLOTS; ++slot)
    {
        if (officer->disliked[slot] == other && other != SIM_NO_OFFICER) return true;
    }
    return false;
}

/* Sworn brothers or spouses (officers-and-relations.md: the full bonus). */
internal b32
sim_officers_bonded(officer_component *a, officer_component *b)
{
    if (!a || !b || a == b) return false;
    if (a->spouse == b->officer_id || b->spouse == a->officer_id) return true;
    return a->sworn_brother_group_head != SIM_NO_OFFICER && a->sworn_brother_group_head == b->sworn_brother_group_head;
}

/* Blood: father and child, or the same father. */
internal b32
sim_officers_blood(officer_component *a, officer_component *b)
{
    if (!a || !b || a == b) return false;
    if (a->father == b->officer_id || b->father == a->officer_id) return true;
    return a->father != SIM_NO_OFFICER && a->father == b->father;
}

internal u32
sim_officer_age(sim_state *sim, officer_component *officer)
{
    u32 year = sim->turns.date.year;
    return year > officer->birth_year ? year - officer->birth_year : 0;
}

/* The item of a type that the officer holds, or 0. */
internal sim_item *
sim_officer_item(sim_state *sim, u16 officer_id, u8 type)
{
    for (u32 index = 0; index < SAVE_ITEM_COUNT; ++index)
    {
        sim_item *item = sim->items + index;
        if (item->exists && item->owner == officer_id && item->type == type) return item;
    }
    return 0;
}

/* True if an officer of the faction holds an item of the type. */
internal b32
sim_faction_has_item(sim_state *sim, u8 force, u8 type)
{
    for (u32 index = 0; index < SAVE_ITEM_COUNT; ++index)
    {
        sim_item *item = sim->items + index;
        if (!item->exists || item->type != type) continue;
        officer_component *owner = sim_find_officer(sim, item->owner);
        if (owner && owner->force_id == force && sim_officer_is_active(owner)) return true;
    }
    return false;
}

/* The items of the save with their kind from items.tsv. */
internal void
sim_load_items(sim_state *sim, save_file *save)
{
    for (u32 index = 0; index < SAVE_ITEM_COUNT; ++index)
    {
        sim_item *item = sim->items + index;
        *item = {};
        item->owner = SIM_NO_OFFICER;
        item->city  = SIM_NO_CITY;
        if (!save || index >= save->item_count) continue;
        item_record *record = save->items + index;
        if (!record->name[0]) continue;
        item->exists = true;
        memcpy(item->name, record->name, sizeof(item->name));
        item->kind     = content_find_item_kind(&sim->content, record->name, ITEM_NAME_SIZE);
        item->type     = item->kind != CONTENT_NO_ITEM_KIND ? sim->content.items[item->kind].type : (u8)ITEM_OTHER;
        item->polearm  = item->kind != CONTENT_NO_ITEM_KIND ? sim->content.items[item->kind].polearm : (u8)POLEARM_PLAIN;
        item->strength = record->effect_strength;
        if (record->owned && sim_find_officer(sim, record->owner_officer_id)) item->owner = record->owner_officer_id;
        else if (record->holding_city < SAVE_CITY_COUNT) item->city = record->holding_city;
    }
}

/* ---- events and small changes --------------------------------------------- */

internal void
sim_emit_officer(sim_state *sim, sim_event_kind kind, officer_component *officer, s32 delta, u32 detail, b32 assumed, u32 cause,
                 sim_event_sink *events)
{
    sim_event event = sim_event_make(sim, kind, cause);
    event.subject = sim_officer_entity(sim, officer->officer_id);
    event.faction = officer->force_id;
    event.delta   = delta;
    event.detail  = detail;
    event.assumed = assumed;
    sim_emit(events, event);
}

internal void
sim_change_loyalty(sim_state *sim, officer_component *officer, s32 change, u32 cause, sim_event_sink *events)
{
    s32 value = (s32)officer->loyalty + change;
    if (value < 0) value = 0;
    /* A save holds loyalty above 100 (113, 242; G2-035). The game keeps the surplus: a rise stops at 100, a loss comes off the surplus. */
    if (value > 100 && change > 0) value = (s32)Maximum((u32)officer->loyalty, 100u);
    s32 delta = value - (s32)officer->loyalty;
    officer->loyalty = (u8)value;
    if (delta) sim_emit_officer(sim, SIM_EVENT_LOYALTY_CHANGED, officer, delta, 0, true, cause, events);
}

internal u8 *
sim_ability_field(officer_component *officer, u32 ability)
{
    switch (ability)
    {
        case CONTENT_ABILITY_LEADERSHIP: return &officer->leadership;
        case CONTENT_ABILITY_FORCE:      return &officer->war;
        case CONTENT_ABILITY_INTELLECT:  return &officer->intelligence;
        case CONTENT_ABILITY_POLITICS:   return &officer->politics;
        case CONTENT_ABILITY_CHARM:      return &officer->charm;
        default:                         return 0;
    }
}

internal void
sim_change_ability(sim_state *sim, officer_component *officer, u32 ability, s32 change, b32 assumed, u32 cause, sim_event_sink *events)
{
    u8 *field = sim_ability_field(officer, ability);
    if (!field) return;
    s32 value = (s32)*field + change;
    if (value < 1) value = 1;
    if (value > 255) value = 255;
    s32 delta = value - (s32)*field;
    *field = (u8)value;
    if (delta) sim_emit_officer(sim, SIM_EVENT_ABILITY_CHANGED, officer, delta, ability, assumed, cause, events);
}

/* Experience from a command raises the ability it uses (manual p.32):
   experience.level points give 1 point, up to 100 (assumed, Q-029). */
internal void
sim_gain_experience(sim_state *sim, officer_component *officer, u32 ability, u32 amount, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    if (!officer || ability >= CONTENT_ABILITY_COUNT || !rules->experience_level) return;
    u32 total = officer->experience[ability] + amount;
    while (total >= rules->experience_level)
    {
        total -= rules->experience_level;
        u8 *field = sim_ability_field(officer, ability);
        if (*field < 100) sim_change_ability(sim, officer, ability, 1, true, cause, events);
    }
    officer->experience[ability] = (u16)total;
}

/* The best of the five abilities of an officer. */
internal u32
sim_best_ability(officer_component *officer)
{
    u32 best = officer->leadership;
    if (officer->war > best)          best = officer->war;
    if (officer->intelligence > best) best = officer->intelligence;
    if (officer->politics > best)     best = officer->politics;
    if (officer->charm > best)        best = officer->charm;
    return best;
}

/* 流放 of an own officer takes technique points: technique.exile, plus the
   same amount scaled by the best ability (tech-tree.md: 5 to 10, more for
   a strong officer; claimed). */
internal u32
sim_exile_technique(u32 best_ability, sim_rules *rules)
{
    return rules->exile_technique + rules->exile_technique * best_ability / 100;
}

/* Gives merit, the technique points of the action and experience after a command. */
internal void
sim_reward_officer(sim_state *sim, u8 force, u16 officer_id, u32 technique, u32 ability, u32 cause, sim_event_sink *events)
{
    u16 officers[SIM_COMMAND_OFFICERS] = { officer_id, SIM_NO_OFFICER, SIM_NO_OFFICER };
    sim_reward_command(sim, force, officers, technique, cause, events);
    sim_gain_experience(sim, sim_find_officer(sim, officer_id), ability, sim->content.rules.experience_command, cause, events);
}

internal void
sim_add_technique_points(sim_state *sim, u8 force, s32 points, u32 cause, sim_event_sink *events)
{
    if (force == SIM_NO_FORCE || !points) return;
    if (points < 0 && (u32)(-points) > sim->technique_points[force]) points = -(s32)sim->technique_points[force];
    sim->technique_points[force] = (u32)((s32)sim->technique_points[force] + points);
    sim_event event = sim_event_make(sim, points > 0 ? SIM_EVENT_TECHNIQUE_GAINED : SIM_EVENT_TECHNIQUE_SPENT, cause);
    event.faction = force;
    event.delta   = points;
    event.assumed = true;
    if (points != 0) sim_emit(events, event);
}

/* ---- posts and ranks ------------------------------------------------------- */

internal u32
sim_faction_city_count(sim_state *sim, u8 force)
{
    u32 count = 0;
    for (u32 index = 0; index < sim->cities.count; ++index) count += sim->cities.data[index].owner_force_id == force;
    return count;
}

/* The highest rank whose city count the faction reaches. The Han emperor
   comes in M8: until then no emperor exists, and the emperor rank needs
   only its cities (factions-corps-and-posts.md). */
internal u8
sim_rank_for(sim_state *sim, u8 force)
{
    u32 cities = sim_faction_city_count(sim, force);
    u8 rank = 0;
    for (u32 index = 0; index < sim->content.rank_count; ++index)
    {
        /* M8: 皇帝 needs the protection of the Han emperor, or no emperor. */
        if (sim->content.ranks[index].needs_emperor && sim->emperor.who != EMPEROR_NONE && sim_emperor_protector(sim) != force) continue;
        if (cities >= sim->content.ranks[index].cities) rank = (u8)index;
    }
    return rank;
}

internal void
sim_update_ranks(sim_state *sim, b32 silent, u32 cause, sim_event_sink *events)
{
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        u8 rank = sim_rank_for(sim, (u8)force);
        if (rank == sim->rank[force]) continue;
        sim->rank[force] = rank;
        if (silent || sim_force_ruler(sim, (u8)force) == SIM_NO_OFFICER) continue;
        sim_event event = sim_event_make(sim, SIM_EVENT_RANK_CHANGED, cause);
        event.faction = (u8)force;
        event.detail  = rank;
        sim_emit(events, event);
    }
}

/* The soldiers that an officer leads: the rank of a ruler, the post of an
   officer, or unit.soldier_cap without a post (Q-169); plus 改革軍制. */
internal u32
sim_officer_soldier_cap(sim_state *sim, officer_component *officer)
{
    sim_rules *rules = &sim->content.rules;
    if (!officer) return rules->unit_soldier_cap;
    u32 cap = rules->unit_soldier_cap;
    if (officer->status == OFFICER_STATUS_RULER && officer->force_id < SIM_FORCE_CAPACITY && sim->rank[officer->force_id] < sim->content.rank_count)
    {
        cap = sim->content.ranks[sim->rank[officer->force_id]].soldiers;
    }
    else if (officer->post < sim->content.post_count)
    {
        cap = sim->content.posts[officer->post].soldiers;
    }
    return cap + sim_technique_effect_value(sim, officer->force_id, TECHNIQUE_EFFECT_SOLDIER_CAP);
}

/* Takes the post and its bonus from an officer. */
internal void
sim_clear_post(sim_state *sim, officer_component *officer)
{
    if (officer->post < sim->content.post_count)
    {
        post_kind *post = sim->content.posts + officer->post;
        u8 *field = sim_ability_field(officer, post->ability);
        if (field) *field = (u8)(*field > officer->post_bonus ? *field - officer->post_bonus : 1);
    }
    officer->post       = CONTENT_NO_POST;
    officer->post_bonus = 0;
}

/* ---- leaving, death and the successor ---------------------------------------- */

/* Takes an officer out of the unit that holds it. A unit without officers
   goes back to its city (assumed). */
internal void
sim_remove_from_unit(sim_state *sim, officer_component *officer, u32 cause, sim_event_sink *events)
{
    if (officer->current_position < SIM_UNIT_POSITION_BASE) return;
    u32 unit_id = officer->current_position - SIM_UNIT_POSITION_BASE;
    sim_unit *unit = sim_find_unit(sim, unit_id);
    if (!unit) return;
    u32 kept = 0;
    u16 officers[SIM_COMMAND_OFFICERS] = { SIM_NO_OFFICER, SIM_NO_OFFICER, SIM_NO_OFFICER };
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        if (unit->officers[place] != officer->officer_id && unit->officers[place] != SIM_NO_OFFICER) officers[kept++] = unit->officers[place];
    }
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) unit->officers[place] = officers[place];
    if (!kept) sim_unit_enter_city(sim, unit_id, unit->home_city, SIM_EVENT_NONE, cause, events);
}

/* The officer leaves its faction and becomes free in its city. */
internal void
sim_make_free(sim_state *sim, officer_component *officer, u32 cause, sim_event_sink *events)
{
    sim_remove_from_unit(sim, officer, cause, events);
    sim_clear_post(sim, officer);
    u16 city = officer->base_location < SAVE_CITY_COUNT ? officer->base_location : 0;
    officer->status           = OFFICER_STATUS_FREE_OR_NOT_APPEARED;
    officer->force_id         = SIM_NO_FORCE;
    officer->current_position = city;
    officer->base_location    = city;
    officer->job_city         = SIM_NO_CITY;
}

/* A new ruler: the active officer of the faction with the most merit. A
   faction without one ends: its cities have no owner (assumed). */
internal void
sim_choose_successor(sim_state *sim, u8 force, u32 cause, sim_event_sink *events)
{
    officer_component *best = 0;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!sim_officer_is_active(officer) || officer->force_id != force) continue;
        if (!best || officer->merit > best->merit) best = officer;
    }
    if (best)
    {
        sim_clear_post(sim, best);
        best->status  = OFFICER_STATUS_RULER;
        best->loyalty = 100;
        sim_emit_officer(sim, SIM_EVENT_RULER_SUCCEEDED, best, 0, 0, false, cause, events);
        return;
    }
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        if (sim->cities.data[index].owner_force_id == force) sim->cities.data[index].owner_force_id = SIM_NO_FORCE;
    }
}

/* M9: the settings of the start that change the officers once
   (game-setup-and-modes.md). 血緣・關係 off clears the historical relations:
   liked, disliked, spouse and sworn brothers (the father stays: it is
   blood). 女武將 off: a female officer outside a faction never appears. */
internal void
sim_apply_start_settings(sim_state *sim, sim_settings *before)
{
    b32 clear_relations = before->relations && !sim->settings.relations;
    b32 hide_females    = before->female_officers && !sim->settings.female_officers;
    if (!clear_relations && !hide_females) return;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (clear_relations)
        {
            for (u32 slot = 0; slot < OFFICER_RELATION_SLOTS; ++slot) officer->liked[slot] = officer->disliked[slot] = SIM_NO_OFFICER;
            officer->spouse                   = SIM_NO_OFFICER;
            officer->sworn_brother_group_head = SIM_NO_OFFICER;
        }
        if (hide_females && officer->female && officer->status == OFFICER_STATUS_FREE_OR_NOT_APPEARED) officer->status = OFFICER_STATUS_DEAD;
    }
}

internal void
sim_officer_dies(sim_state *sim, officer_component *officer, u32 cause, sim_event_sink *events)
{
    b32 ruler = officer->status == OFFICER_STATUS_RULER;
    u8 force = officer->force_id;
    sim_remove_from_unit(sim, officer, cause, events);
    sim_clear_post(sim, officer);
    for (u32 index = 0; index < SAVE_ITEM_COUNT; ++index)
    {
        sim_item *item = sim->items + index;
        if (!item->exists || item->owner != officer->officer_id) continue;
        item->owner = SIM_NO_OFFICER;
        item->city  = officer->base_location < SAVE_CITY_COUNT ? (u8)officer->base_location : SIM_NO_CITY;
    }
    officer->status = OFFICER_STATUS_DEAD;
    sim_emit_officer(sim, SIM_EVENT_OFFICER_DIED, officer, 0, 0, false, cause, events);
    if (ruler && force != SIM_NO_FORCE) sim_choose_successor(sim, force, cause, events);
}

/* ---- the calendar ------------------------------------------------------------ */

/* Each turn: stamina and wounds recover; the aid units count down. */
internal void
sim_run_officers_turn(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->status == OFFICER_STATUS_DEAD) continue;
        officer->stamina = (u8)Minimum(100u, (u32)officer->stamina + rules->stamina_recovery);
        if (officer->health == HEALTH_GOOD) continue;
        if (officer->health_turns > 1) { officer->health_turns--; continue; }
        officer->health--;
        officer->health_turns = (u8)(officer->health ? rules->wound_turns : 0);
        sim_emit_officer(sim, SIM_EVENT_HEALTH_CHANGED, officer, officer->health, 0, true, cause, events);
    }
    for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
    {
        sim_unit *unit = sim->units + unit_id;
        if (!unit->alive || !unit->aid_turns) continue;
        if (--unit->aid_turns) continue;
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
        {
            officer_component *officer = sim_find_officer(sim, unit->officers[place]);
            if (!officer) continue;
            u8 refuge = sim_refuge_city(sim, unit->force_id, unit->home_city);
            officer->current_position = refuge == SIM_NO_CITY ? 0 : refuge;
        }
        unit->alive = false;
        sim_event left = sim_event_make(sim, SIM_EVENT_AID_LEFT, cause);
        left.faction = unit->force_id;
        left.detail  = unit_id;
        sim_emit(events, left);
    }
}

/* The loss of loyalty of one month (assumed, Q-028): loyalty.monthly, more
   for ambition above normal and less for justice above normal; half with
   仁政 in the city; none for the sworn brothers, spouses and liked officers
   of the ruler. */
internal u32
sim_monthly_loyalty_loss(sim_state *sim, officer_component *officer, officer_component *ruler)
{
    sim_rules *rules = &sim->content.rules;
    if (ruler && (sim_officers_bonded(officer, ruler) || sim_officer_likes(ruler, officer->officer_id))) return 0;
    s32 loss = (s32)rules->loyalty_monthly + ((s32)officer->ambition - 2) * (s32)rules->loyalty_ambition -
               ((s32)officer->justice - 2) * (s32)rules->loyalty_ambition;
    if (sim_has_technique_effect(sim, officer->force_id, TECHNIQUE_EFFECT_LOYALTY_KEEP, CONTENT_CLASS_NONE)) loss /= 2;
    if (officer->current_position < SAVE_CITY_COUNT && sim_city_has_skill(sim, (u8)officer->current_position, rules->skill_benevolence)) loss /= 2;
    return loss > 0 ? (u32)loss : 0;
}

/* Each month: the loyalty, the officers that leave, the ranks, the seal,
   and the months of the ceasefires. */
internal void
sim_run_officers_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_run_deaths_month(sim, cause, events);   /* G2-055 */
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!sim_officer_is_active(officer) || officer->status == OFFICER_STATUS_RULER || officer->force_id == SIM_NO_FORCE) continue;
        officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, officer->force_id));
        sim_loyalty_drift_fn *drift = sim->ruleset.loyalty_drift ? sim->ruleset.loyalty_drift : sim_monthly_loyalty_loss;   /* the rule hook */
        u32 loss = drift(sim, officer, ruler);
        if (loss) sim_change_loyalty(sim, officer, -(s32)loss, cause, events);
        b32 bonded = ruler && sim_officers_bonded(officer, ruler);
        if (officer->loyalty < rules->loyalty_leave && officer->justice < 4 && !bonded &&
            sim_random_below(sim, 100) < rules->loyalty_leave_chance)
        {
            u8 old = officer->force_id;
            sim_make_free(sim, officer, cause, events);
            sim_emit_officer(sim, SIM_EVENT_OFFICER_LEFT, officer, 0, old, true, cause, events);
        }
    }
    sim_update_ranks(sim, false, cause, events);
    for (u32 index = 0; index < SAVE_ITEM_COUNT; ++index)
    {
        sim_item *item = sim->items + index;
        officer_component *owner = sim_find_officer(sim, item->owner);
        if (item->exists && item->type == ITEM_SEAL && owner && sim_officer_is_active(owner))
        {
            sim_add_technique_points(sim, owner->force_id, (s32)rules->seal_technique, cause, events);
        }
    }
    sim_run_diplomacy_month(sim, cause, events);
}

/* The age of decline of each growth type (guide book p.51); 晚成 never. */
internal u32
sim_decline_age(u8 growth)
{
    switch (growth)
    {
        case GROWTH_EARLY:     return 35;
        case GROWTH_SUSTAINED: return 50;
        case GROWTH_LATE:      return 1000;
        default:               return 45;
    }
}

/* Each year: force and leadership fall after the age of decline (with
   能力變動), and old officers die (with the historical lifespan). */
internal void
sim_run_officers_year(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    u32 year = sim->turns.date.year;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->status == OFFICER_STATUS_DEAD || officer->status == OFFICER_STATUS_EMPTY_SLOT || officer->appearance_year > year) continue;
        if (sim->settings.ability_change && sim_officer_age(sim, officer) > sim_decline_age(officer->growth) && rules->age_decline)
        {
            sim_change_ability(sim, officer, CONTENT_ABILITY_FORCE, -(s32)rules->age_decline, true, cause, events);
            sim_change_ability(sim, officer, CONTENT_ABILITY_LEADERSHIP, -(s32)rules->age_decline, true, cause, events);
        }
    }
}

/* G2-055: at each month start an officer past its death year rolls its
   death (朱儁, death year 195, died at the change to 197/12/1). The chance
   is assumed (Q-029). With 壽命 長 the death year is later (M9). */
internal void
sim_run_deaths_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    u32 year = sim->turns.date.year;
    if (!sim->settings.lifespan || !rules->death_month_chance) return;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->status == OFFICER_STATUS_DEAD || officer->status == OFFICER_STATUS_EMPTY_SLOT || officer->appearance_year > year) continue;
        u32 death_year = officer->death_year + (sim->settings.long_lifespan ? rules->lifespan_long_years : 0);
        if (officer->death_year && year > death_year && sim_random_below(sim, 100) < rules->death_month_chance)
        {
            sim_officer_dies(sim, officer, cause, events);
        }
    }
}

/* ---- the checks of the personnel and ruler commands ------------------------------ */

/* The checks that every M7 command of a city shares. */
internal sim_refusal
sim_check_court_common(sim_state *sim, u8 city_id, city_component **city_out)
{
    if (!sim->has_content) return SIM_REFUSAL_NO_CONTENT;
    if (sim_game_over(sim)) return SIM_REFUSAL_GAME_OVER;
    if (sim->duel.active || sim->debate.active) return SIM_REFUSAL_BUSY;
    city_component *city = sim_find_city(sim, city_id);
    if (!city || city->owner_force_id == SIM_NO_FORCE) return SIM_REFUSAL_NO_CITY;
    if (sim_whose_turn(sim) != city->owner_force_id) return SIM_REFUSAL_NOT_YOUR_TURN;
    *city_out = city;
    return SIM_REFUSAL_NONE;
}

/* The action points of a command, halved by an office of the effect in
   the city (PK, city-commands.md). */
internal u32
sim_office_ap(sim_state *sim, city_component *city, facility_effect office, u32 action_points)
{
    return sim_city_has_built(sim, city->city_id, office) ? action_points / 2 : action_points;
}

internal sim_check
sim_check_points(sim_state *sim, u8 force, sim_check check)
{
    if (sim->turns.action_points[force] < check.action_points) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    return check;
}

/* The loyalty that one reward gives (assumed, Q-028). The spec names one
   factor: an officer near the compatibility of the ruler gets more
   (officers-and-relations.md, "Relations"). The rise is reward.loyalty,
   less one for each reward.compatibility_divisor of the distance on the
   wheel, and at least reward.loyalty_min. */
internal u32
sim_reward_loyalty_rise(sim_rules *rules, u32 compatibility_distance)
{
    u32 penalty = rules->reward_compatibility_divisor ? compatibility_distance / rules->reward_compatibility_divisor : 0;
    u32 rise    = rules->reward_loyalty > penalty ? rules->reward_loyalty - penalty : 0;
    return rise < rules->reward_loyalty_min ? rules->reward_loyalty_min : rise;
}

/* The rise of one reward of the faction for the officer. */
internal u32
sim_reward_rise_of(sim_state *sim, u8 force, officer_component *officer)
{
    officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, force));
    u32 distance = ruler ? sim_compatibility_distance(officer->compatibility, ruler->compatibility) : 0;
    return sim_reward_loyalty_rise(&sim->content.rules, distance);
}

/* Whether the faction can reward the officer now (city-commands.md, 褒賞:
   not at loyalty 100, not for an officer in a unit, one time in a turn).
   The ruler gets no reward. */
internal sim_refusal
sim_check_reward_officer(sim_state *sim, u8 force, officer_component *officer)
{
    if (!officer || !sim_officer_is_active(officer) || officer->force_id != force) return SIM_REFUSAL_OFFICER_NOT_IN_CITY;
    if (officer->current_position >= SIM_UNIT_POSITION_BASE || officer->training_turns) return SIM_REFUSAL_OFFICER_NOT_IN_CITY;
    if (officer->loyalty >= 100 || officer->status == OFFICER_STATUS_RULER) return SIM_REFUSAL_VALUE_AT_MAXIMUM;
    if (officer->rewarded_turn == sim->turn + 1) return SIM_REFUSAL_LIMIT_REACHED;
    return SIM_REFUSAL_NONE;
}

/* The band of a chance in percent for the clue of the adviser (assumed). */
internal u8
sim_chance_clue(sim_rules *rules, u32 chance)
{
    if (chance < rules->clue_low_below) return SIM_CLUE_LOW;
    if (chance >= rules->clue_high_from) return SIM_CLUE_HIGH;
    return SIM_CLUE_MIDDLE;
}

/* The free officer that a search in the city can find, or 0. */
internal officer_component *
sim_search_target(sim_state *sim, city_component *city)
{
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->status == OFFICER_STATUS_FREE_OR_NOT_APPEARED && officer->base_location == city->city_id &&
            officer->appearance_year <= sim->turns.date.year)
        {
            return officer;
        }
    }
    return 0;
}

/* The chance of a search in percent (assumed, Q-178): search.base plus a
   part of the politics, 100 with 眼力, and 0 when the city hides no free
   officer. */
internal u32
sim_search_chance(sim_state *sim, city_component *city, officer_component *searcher)
{
    sim_rules *rules = &sim->content.rules;
    if (!sim_search_target(sim, city)) return 0;
    if (sim_officer_has_skill(searcher, rules->skill_search)) return 100;
    u32 chance = rules->search_base + searcher->politics * rules->search_politics_percent / 100;
    return chance > 100 ? 100 : chance;
}

/* The chance of a recruit, in percent (assumed, Q-178). */
internal u32
sim_recruit_chance(sim_state *sim, officer_component *recruiter, officer_component *target)
{
    sim_rules *rules = &sim->content.rules;
    officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, recruiter->force_id));
    if (ruler && sim_officers_bonded(ruler, target)) return 100;   /* the elder recruits a sworn brother or a spouse */
    s32 chance = (s32)rules->recruit_base + (s32)(recruiter->charm * rules->recruit_charm_percent / 100);
    b32 free_officer = target->status == OFFICER_STATUS_FREE_OR_NOT_APPEARED;
    chance += free_officer ? 50 : 100 - (s32)target->loyalty;
    if (ruler && rules->recruit_compatibility_divisor)
    {
        chance -= (s32)(sim_compatibility_distance(target->compatibility, ruler->compatibility) / rules->recruit_compatibility_divisor);
    }
    if (target->status == SIM_STATUS_CAPTIVE) chance += (s32)rules->recruit_captive_bonus;
    if (sim_officers_blood(recruiter, target) || sim_officer_likes(target, recruiter->officer_id) ||
        (ruler && (sim_officers_blood(ruler, target) || sim_officer_likes(target, ruler->officer_id))))
    {
        chance += (s32)rules->recruit_relation_bonus;
    }
    return (u32)(chance < 0 ? 0 : (chance > 100 ? 100 : chance));
}

/* Who can be recruited (officers-and-relations.md, "Recruitable officers"). */
internal sim_refusal
sim_check_recruit_target(sim_state *sim, u8 force, officer_component *target)
{
    if (!target || target->status == OFFICER_STATUS_DEAD || target->status == OFFICER_STATUS_EMPTY_SLOT) return SIM_REFUSAL_NO_OFFICER;
    if (target->appearance_year > sim->turns.date.year) return SIM_REFUSAL_NO_OFFICER;
    officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, force));
    if (ruler && sim_officer_dislikes(target, ruler->officer_id)) return SIM_REFUSAL_NOT_RECRUITABLE;
    if (target->status == SIM_STATUS_CAPTIVE) return target->force_id == force ? SIM_REFUSAL_NONE : SIM_REFUSAL_NOT_RECRUITABLE;
    if (target->status == OFFICER_STATUS_FREE_OR_NOT_APPEARED)
    {
        city_component *city = target->base_location < SAVE_CITY_COUNT ? sim_find_city(sim, (u8)target->base_location) : 0;
        if (!city || (city->owner_force_id != force && city->owner_force_id != SIM_NO_FORCE)) return SIM_REFUSAL_NOT_RECRUITABLE;
        return SIM_REFUSAL_NONE;
    }
    if (target->force_id == force || target->status == OFFICER_STATUS_RULER || target->loyalty >= 100) return SIM_REFUSAL_NOT_RECRUITABLE;
    if (target->current_position >= SIM_UNIT_POSITION_BASE) return SIM_REFUSAL_NOT_RECRUITABLE;
    officer_component *their_ruler = sim_find_officer(sim, sim_force_ruler(sim, target->force_id));
    if (their_ruler && sim_officers_bonded(target, their_ruler)) return SIM_REFUSAL_NOT_RECRUITABLE;
    return SIM_REFUSAL_NONE;
}

/* An item that the faction holds: an officer of the faction has it, or a
   city of the faction keeps it. */
internal b32
sim_faction_holds_item(sim_state *sim, u8 force, u32 item_id)
{
    if (item_id >= SAVE_ITEM_COUNT || !sim->items[item_id].exists) return false;
    sim_item *item = sim->items + item_id;
    officer_component *owner = sim_find_officer(sim, item->owner);
    if (owner) return owner->force_id == force && sim_officer_is_active(owner);
    city_component *city = sim_find_city(sim, item->city);
    return city && city->owner_force_id == force;
}

/* An active officer of the faction (anywhere), or 0. */
internal officer_component *
sim_own_officer(sim_state *sim, u8 force, u16 officer_id)
{
    officer_component *officer = sim_find_officer(sim, officer_id);
    return officer && sim_officer_is_active(officer) && officer->force_id == force ? officer : 0;
}

internal sim_check
sim_check_matchmake(sim_state *sim, u8 force, sim_court_order *order, sim_check check)
{
    sim_rules *rules = &sim->content.rules;
    officer_component *a = sim_own_officer(sim, force, order->officer), *b = sim_own_officer(sim, force, order->target);
    if (!a || !b || a == b) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    if (a->merit < rules->matchmaking_merit || b->merit < rules->matchmaking_merit) return sim_refuse(check, SIM_REFUSAL_NO_MERIT);
    if (sim->technique_points[force] < rules->matchmaking_technique) return sim_refuse(check, SIM_REFUSAL_NO_TECHNIQUE_POINTS);
    if (sim_officer_dislikes(a, b->officer_id) || sim_officer_dislikes(b, a->officer_id)) return sim_refuse(check, SIM_REFUSAL_NOT_MATCHABLE);
    if (order->kind == SIM_MATCH_SWORN)
    {
        b32 same_group = a->sworn_brother_group_head != SIM_NO_OFFICER && a->sworn_brother_group_head == b->sworn_brother_group_head;
        if (a->female != b->female || same_group) return sim_refuse(check, SIM_REFUSAL_NOT_MATCHABLE);
    }
    else if (order->kind == SIM_MATCH_MARRIAGE)
    {
        if (a->female == b->female || a->spouse != SIM_NO_OFFICER || b->spouse != SIM_NO_OFFICER ||
            sim_officer_age(sim, a) >= rules->matchmaking_age || sim_officer_age(sim, b) >= rules->matchmaking_age)
        {
            return sim_refuse(check, SIM_REFUSAL_NOT_MATCHABLE);
        }
    }
    else return sim_refuse(check, SIM_REFUSAL_NOT_MATCHABLE);
    check.quantity = rules->matchmaking_technique;   /* the technique points that it costs */
    return check;
}

internal sim_check
sim_check_appoint(sim_state *sim, u8 force, sim_court_order *order, sim_check check)
{
    officer_component *officer = sim_own_officer(sim, force, order->target);
    if (!officer || officer->status == OFFICER_STATUS_RULER) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    if (order->post == CONTENT_NO_POST) return check;   /* takes the post away */
    if (order->post >= sim->content.post_count) return sim_refuse(check, SIM_REFUSAL_NO_POST);
    post_kind *post = sim->content.posts + order->post;
    if (officer->merit < post->merit)  return sim_refuse(check, SIM_REFUSAL_NO_MERIT);
    if (post->rank > sim->rank[force]) return sim_refuse(check, SIM_REFUSAL_NO_RANK);
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *other = sim->officers.data + index;
        if (other != officer && other->force_id == force && sim_officer_is_active(other) && other->post == order->post)
        {
            return sim_refuse(check, SIM_REFUSAL_POST_TAKEN);
        }
    }
    check.quantity = post->soldiers;
    return check;
}

/* The checks of the M7 commands of officers: search, recruit, reward,
   give item, appoint, matchmake, release and exile. */
internal sim_check
sim_check_court(sim_state *sim, sim_command_kind kind, sim_court_order *order)
{
    sim_check check = {};
    check.slot    = SIM_ANY_SLOT;
    check.assumed = true;
    sim_rules *rules = &sim->content.rules;
    city_component *city = 0;
    sim_refusal refusal = sim_check_court_common(sim, order->city_id, &city);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    u8 force = city->owner_force_id;
    switch (kind)
    {
        case SIM_COMMAND_SEARCH:
        case SIM_COMMAND_RECRUIT:
        {
            refusal = sim_check_develop_officer(sim, city, sim_find_officer(sim, order->officer));
            if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
            check.action_points = sim_office_ap(sim, city, FACILITY_EFFECT_PERSONNEL, kind == SIM_COMMAND_SEARCH ? rules->ap_search : rules->ap_recruit);
            if (kind == SIM_COMMAND_SEARCH) check.quantity = sim_search_chance(sim, city, sim_find_officer(sim, order->officer));
            if (kind == SIM_COMMAND_RECRUIT)
            {
                officer_component *target = sim_find_officer(sim, order->target);
                refusal = sim_check_recruit_target(sim, force, target);
                if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
                check.quantity = sim_recruit_chance(sim, sim_find_officer(sim, order->officer), target);
            }
            return sim_check_points(sim, force, check);
        }
        case SIM_COMMAND_REWARD:
        {
            /* G2-040: a set of officers of the whole force. The quantity is the loyalty that the set gets. */
            if (!order->reward_count || order->reward_count > SIM_REWARD_OFFICERS) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
            for (u32 place = 0; place < order->reward_count; ++place)
            {
                officer_component *officer = sim_find_officer(sim, order->rewards[place]);
                refusal = sim_check_reward_officer(sim, force, officer);
                if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
                for (u32 earlier = 0; earlier < place; ++earlier)
                {
                    if (order->rewards[earlier] == order->rewards[place]) return sim_refuse(check, SIM_REFUSAL_DUPLICATE_OFFICER);
                }
                check.quantity += sim_reward_rise_of(sim, force, officer);
            }
            check.gold          = rules->reward_gold * order->reward_count;
            check.action_points = rules->ap_reward * order->reward_count;
            if (city->gold < check.gold) return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
            return sim_check_points(sim, force, check);
        }
        case SIM_COMMAND_GIVE_ITEM:
        {
            officer_component *target = sim_own_officer(sim, force, order->target);
            if (!target) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
            if (!sim_faction_holds_item(sim, force, order->item) || sim->items[order->item].owner == order->target)
            {
                return sim_refuse(check, SIM_REFUSAL_NO_ITEM);
            }
            check.action_points = rules->ap_give_item;
            check.quantity      = sim->items[order->item].strength;
            return sim_check_points(sim, force, check);
        }
        case SIM_COMMAND_APPOINT:   return sim_check_appoint(sim, force, order, check);
        case SIM_COMMAND_MATCHMAKE: return sim_check_matchmake(sim, force, order, check);
        case SIM_COMMAND_RELEASE:
        {
            officer_component *target = sim_find_officer(sim, order->target);
            if (!target || target->status != SIM_STATUS_CAPTIVE || target->force_id != force) return sim_refuse(check, SIM_REFUSAL_NOT_CAPTIVE);
            check.quantity = rules->release_technique;
            return check;
        }
        case SIM_COMMAND_EXILE:
        {
            officer_component *target = sim_own_officer(sim, force, order->target);
            if (!target || target->status == OFFICER_STATUS_RULER) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
            check.quantity = sim_exile_technique(sim_best_ability(target), rules);
            return check;
        }
        default:
            return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    }
}

/* ---- the effects -------------------------------------------------------------- */

/* The officer joins the faction in the city (a recruit, a search, a won
   debate). */
internal void
sim_join_faction(sim_state *sim, officer_component *target, u8 force, u8 city_id, u32 cause, sim_event_sink *events)
{
    u8 old = target->status == SIM_STATUS_CAPTIVE ? target->former_force : target->force_id;
    sim_remove_from_unit(sim, target, cause, events);
    sim_clear_post(sim, target);
    target->force_id         = force;
    target->status           = OFFICER_STATUS_REGULAR_OFFICER;
    target->current_position = city_id;
    target->base_location    = city_id;
    target->loyalty          = (u8)Minimum(sim->content.rules.recruit_new_loyalty, 100u);
    target->former_force     = SIM_NO_FORCE;
    target->job_city         = SIM_NO_CITY;
    target->acted_turn       = sim->turn + 1;
    sim_emit_officer(sim, SIM_EVENT_OFFICER_RECRUITED, target, 0, old, true, cause, events);
    /* An officer recruited away from an ally lowers the friendliness (guide book p.12). */
    if (old != SIM_NO_FORCE && old != force && old < SIM_DIPLOMACY_FORCES) sim_change_friendliness(sim, force, old, -(s32)sim->content.rules.diplomacy_fail_loss, cause, events);
}

internal void
sim_search(sim_state *sim, city_component *city, sim_court_order *order, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    officer_component *searcher = sim_find_officer(sim, order->officer);
    officer_component *found = sim_search_target(sim, city);
    u32 chance = sim_search_chance(sim, city, searcher);
    b32 success = found && (chance >= 100 || sim_random_below(sim, 100) < chance);
    if (success)
    {
        sim_event event = sim_event_make(sim, SIM_EVENT_OFFICER_FOUND, cause);
        event.subject = sim_officer_entity(sim, found->officer_id);
        event.faction = city->owner_force_id;
        event.assumed = true;
        sim_emit(events, event);
        if (found->intelligence >= rules->search_debate_intellect)
        {
            sim_court_order pending = *order;
            pending.target = found->officer_id;
            sim_begin_debate(sim, searcher->officer_id, found->officer_id, DEBATE_FOR_SEARCH, &pending, city->owner_force_id, cause, events);
        }
        else if (sim_random_below(sim, 100) < sim_recruit_chance(sim, searcher, found))
        {
            sim_join_faction(sim, found, city->owner_force_id, city->city_id, cause, events);
        }
    }
    else if (!found && sim_random_below(sim, 100) < rules->search_gold_chance)
    {
        success = true;
        city->gold += rules->search_gold;
        sim_emit_city(sim, SIM_EVENT_GOLD_FOUND, city, (s32)rules->search_gold, 0, true, cause, events);
    }
    /* G2-039: a search with no find gives the log line 蒯良在江陵的探索失敗. */
    if (!success) sim_emit_city(sim, SIM_EVENT_SEARCH_FAILED, city, 0, 0, true, cause, events);
    /* 探索 gives technique points only when it finds an officer (tech-tree.md). */
    sim_reward_officer(sim, city->owner_force_id, searcher->officer_id, found && success ? rules->technique_search_found : 0u,
                       CONTENT_ABILITY_POLITICS, cause, events);
    if (!sim->debate.active) sim_events_after_search(sim, city->city_id, searcher->officer_id, cause, events);   /* M8: the travellers */
}

internal void
sim_recruit(sim_state *sim, city_component *city, sim_court_order *order, sim_check *check, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    officer_component *recruiter = sim_find_officer(sim, order->officer);
    officer_component *target    = sim_find_officer(sim, order->target);
    b32 success = false;
    if (target->intelligence >= rules->recruit_debate_intellect && target->status != SIM_STATUS_CAPTIVE && check->quantity < 100)
    {
        sim_begin_debate(sim, recruiter->officer_id, target->officer_id, DEBATE_FOR_RECRUIT, order, city->owner_force_id, cause, events);
    }
    else
    {
        success = check->quantity >= 100 || sim_random_below(sim, 100) < check->quantity;
        if (success) sim_join_faction(sim, target, city->owner_force_id, city->city_id, cause, events);
        else         sim_emit_officer(sim, SIM_EVENT_RECRUIT_FAILED, target, 0, 0, true, cause, events);
    }
    sim_reward_officer(sim, city->owner_force_id, recruiter->officer_id, success ? rules->technique_recruit : 0u, CONTENT_ABILITY_CHARM, cause, events);
}

internal void
sim_appoint(sim_state *sim, sim_court_order *order, u32 cause, sim_event_sink *events)
{
    officer_component *officer = sim_find_officer(sim, order->target);
    sim_clear_post(sim, officer);
    if (order->post < sim->content.post_count)
    {
        post_kind *post = sim->content.posts + order->post;
        officer->post = order->post;
        u8 *field = sim_ability_field(officer, post->ability);
        if (field)
        {
            u32 bonus = Minimum(post->bonus, 255u - *field);
            *field = (u8)(*field + bonus);
            officer->post_bonus = (u8)bonus;
        }
    }
    sim_emit_officer(sim, SIM_EVENT_POST_APPOINTED, officer, 0, order->post, false, cause, events);
}

internal void
sim_matchmake(sim_state *sim, u8 force, sim_court_order *order, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    officer_component *a = sim_find_officer(sim, order->officer), *b = sim_find_officer(sim, order->target);
    sim_add_technique_points(sim, force, -(s32)rules->matchmaking_technique, cause, events);
    if (order->kind == SIM_MATCH_SWORN)
    {
        u16 head = a->sworn_brother_group_head != SIM_NO_OFFICER ? a->sworn_brother_group_head :
                   b->sworn_brother_group_head != SIM_NO_OFFICER ? b->sworn_brother_group_head :
                   (a->birth_year <= b->birth_year ? a->officer_id : b->officer_id);   /* the elder leads the group */
        u16 old_a = a->sworn_brother_group_head, old_b = b->sworn_brother_group_head;
        for (u32 index = 0; index < sim->officers.count; ++index)
        {
            officer_component *officer = sim->officers.data + index;
            b32 in_group = officer->sworn_brother_group_head != SIM_NO_OFFICER &&
                           (officer->sworn_brother_group_head == old_a || officer->sworn_brother_group_head == old_b);
            if (in_group) officer->sworn_brother_group_head = head;
        }
        a->sworn_brother_group_head = head;
        b->sworn_brother_group_head = head;
        sim_emit_officer(sim, SIM_EVENT_SWORN, a, 0, b->officer_id, false, cause, events);
        return;
    }
    a->spouse = b->officer_id;
    b->spouse = a->officer_id;
    sim_emit_officer(sim, SIM_EVENT_MARRIED, a, 0, b->officer_id, false, cause, events);
    /* 內助 #100: both officers gain 1 in each ability (Steam guide, skills.md). */
    if (sim_officer_has_skill(a, rules->skill_wife_help) || sim_officer_has_skill(b, rules->skill_wife_help))
    {
        officer_component *pair[2] = { a, b };
        for (u32 side = 0; side < 2; ++side)
        {
            pair[side]->wife_bonus = 1;
            for (u32 ability = 0; ability < CONTENT_ABILITY_COUNT; ++ability) sim_change_ability(sim, pair[side], ability, 1, false, cause, events);
        }
    }
}

/* A captive goes back to its force, to a refuge city, or becomes free
   when the force is gone. kind says why: a release or an escape. */
internal void
sim_free_captive(sim_state *sim, officer_component *officer, sim_event_kind kind, u32 cause, sim_event_sink *events)
{
    u8 force = officer->former_force;
    u8 refuge = force != SIM_NO_FORCE ? sim_refuge_city(sim, force, SIM_NO_CITY) : SIM_NO_CITY;
    if (refuge == SIM_NO_CITY)
    {
        sim_make_free(sim, officer, cause, events);
    }
    else
    {
        officer->force_id         = force;
        officer->status           = OFFICER_STATUS_REGULAR_OFFICER;
        officer->current_position = refuge;
        officer->base_location    = refuge;
    }
    officer->former_force = SIM_NO_FORCE;
    sim_emit_officer(sim, kind, officer, 0, 0, kind == SIM_EVENT_CAPTIVE_ESCAPED, cause, events);
}

internal void
sim_release_captive(sim_state *sim, officer_component *officer, u32 cause, sim_event_sink *events)
{
    sim_free_captive(sim, officer, SIM_EVENT_CAPTIVE_RELEASED, cause, events);
}

/* 俘虜處置: the check of the choice of the player for a captive. The
   faction that acts must hold the captive. 登庸 gives the chance of a
   recruit by the ruler in quantity (assumed: the spec names no recruiter
   for the dialog), 釋放 the technique points of a release. */
internal sim_check
sim_check_captive(sim_state *sim, sim_court_order *order)
{
    sim_check check = {};
    check.slot    = SIM_ANY_SLOT;
    check.assumed = true;
    u8 force = sim_whose_turn(sim);
    if (force == SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NOT_YOUR_TURN);
    officer_component *target = sim_find_officer(sim, order->target);
    if (!target || target->status != SIM_STATUS_CAPTIVE || target->force_id != force) return sim_refuse(check, SIM_REFUSAL_NOT_CAPTIVE);
    if (order->kind == SIM_CAPTIVE_RECRUIT)
    {
        officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, force));
        if (!ruler) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
        sim_refusal refusal = sim_check_recruit_target(sim, force, target);
        if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
        check.quantity = sim_recruit_chance(sim, ruler, target);
    }
    if (order->kind == SIM_CAPTIVE_RELEASE) check.quantity = sim->content.rules.release_technique;
    return check;
}

internal void
sim_captive_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_court_order *order = &command.court;
    sim_check check = sim_check_captive(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    u8 force = sim_whose_turn(sim);
    officer_component *target = sim_find_officer(sim, order->target);
    switch (order->kind)
    {
        case SIM_CAPTIVE_RECRUIT:
        {
            b32 success = check.quantity >= 100 || sim_random_below(sim, 100) < check.quantity;
            u8 city = target->base_location < SAVE_CITY_COUNT ? (u8)target->base_location : sim_refuge_city(sim, force, SIM_NO_CITY);
            if (success) sim_join_faction(sim, target, force, city, command.id, events);
            else         sim_emit_officer(sim, SIM_EVENT_RECRUIT_FAILED, target, 0, 0, true, command.id, events);
        } break;
        case SIM_CAPTIVE_RELEASE:
        {
            sim_release_captive(sim, target, command.id, events);
            sim_add_technique_points(sim, force, (s32)sim->content.rules.release_technique, command.id, events);
        } break;
        case SIM_CAPTIVE_EXECUTE:
        {
            sim_emit_officer(sim, SIM_EVENT_OFFICER_EXECUTED, target, 0, 0, true, command.id, events);
            sim_officer_dies(sim, target, command.id, events);
        } break;
        default:
        {
            sim_emit_officer(sim, SIM_EVENT_CAPTIVE_KEPT, target, 0, 0, true, command.id, events);
        } break;
    }
}

/* G2-032: at a month start, each captive rolls an escape with the chance
   captive.escape_percent (assumed). The original showed two escapes at
   one month start, each with a message and a log line. */
internal void
sim_run_captives_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    u32 chance = sim->content.rules.captive_escape_percent;
    if (!chance) return;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->status != SIM_STATUS_CAPTIVE) continue;
        if (sim_random_below(sim, 100) >= chance) continue;
        sim_free_captive(sim, officer, SIM_EVENT_CAPTIVE_ESCAPED, cause, events);
    }
}

/* Runs one of the M7 commands of officers. It checks first: a refused order
   changes nothing. */
internal void
sim_court_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_court_order *order = &command.court;
    sim_check check = sim_check_court(sim, command.kind, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    sim_rules *rules = &sim->content.rules;
    city_component *city = sim_find_city(sim, order->city_id);
    u8 force = city->owner_force_id;
    officer_component *actor = sim_find_officer(sim, order->officer);
    if (actor && (command.kind == SIM_COMMAND_SEARCH || command.kind == SIM_COMMAND_RECRUIT)) actor->acted_turn = sim->turn + 1;
    if (check.gold)
    {
        city->gold -= check.gold;
        sim_emit_city(sim, SIM_EVENT_GOLD_SPENT, city, -(s32)check.gold, 0, false, command.id, events);
    }
    if (check.action_points)
    {
        sim->turns.action_points[force] -= check.action_points;
        sim_event points = sim_event_make(sim, SIM_EVENT_ACTION_POINTS_SPENT, command.id);
        points.faction = force;
        points.delta   = -(s32)check.action_points;
        sim_emit(events, points);
    }
    switch (command.kind)
    {
        case SIM_COMMAND_SEARCH:  sim_search(sim, city, order, command.id, events); break;
        case SIM_COMMAND_RECRUIT: sim_recruit(sim, city, order, &check, command.id, events); break;
        case SIM_COMMAND_REWARD:
        {
            for (u32 place = 0; place < order->reward_count; ++place)
            {
                officer_component *officer = sim_find_officer(sim, order->rewards[place]);
                if (!officer) continue;
                u32 rise = sim_reward_rise_of(sim, force, officer);
                officer->rewarded_turn = sim->turn + 1;
                sim_change_loyalty(sim, officer, (s32)rise, command.id, events);
            }
            break;
        }
        case SIM_COMMAND_GIVE_ITEM:
        {
            sim_item *item = sim->items + order->item;
            officer_component *target = sim_find_officer(sim, order->target);
            item->owner = order->target;
            item->city  = SIM_NO_CITY;
            sim_emit_officer(sim, SIM_EVENT_ITEM_GIVEN, target, 0, order->item, false, command.id, events);
            sim_change_loyalty(sim, target, (s32)item->strength, command.id, events);
            break;
        }
        case SIM_COMMAND_APPOINT:   sim_appoint(sim, order, command.id, events); break;
        case SIM_COMMAND_MATCHMAKE: sim_matchmake(sim, force, order, command.id, events); break;
        case SIM_COMMAND_RELEASE:
            sim_release_captive(sim, sim_find_officer(sim, order->target), command.id, events);
            sim_add_technique_points(sim, force, (s32)rules->release_technique, command.id, events);
            break;
        case SIM_COMMAND_EXILE:
        {
            officer_component *target = sim_find_officer(sim, order->target);
            sim_make_free(sim, target, command.id, events);
            sim_emit_officer(sim, SIM_EVENT_OFFICER_EXILED, target, 0, force, false, command.id, events);
            sim_add_technique_points(sim, force, -(s32)sim_exile_technique(sim_best_ability(target), rules), command.id, events);
            break;
        }
        default: break;
    }
}

internal b32
sim_busy(sim_state *sim)
{
    return sim->duel.active || sim->debate.active || sim->pending.active;   /* M8: an event that waits for a choice */
}

/* The officers of a city that cannot pay the salaries lose loyalty
   (manual p.11; the amount is assumed, Q-028). */
internal void
sim_unpaid_loyalty(sim_state *sim, city_component *city, u32 cause, sim_event_sink *events)
{
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!sim_officer_is_active(officer) || officer->status == OFFICER_STATUS_RULER || officer->force_id != city->owner_force_id ||
            officer->base_location != city->city_id) continue;
        sim_change_loyalty(sim, officer, -(s32)sim->content.rules.loyalty_unpaid, cause, events);
    }
}
