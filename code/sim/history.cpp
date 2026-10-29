/* The historical events, the endings and the portraits of M9 (events.md).
   The historical events are data (history.tsv): each row has conditions
   and effects in a small clause language, and this file is the one
   interpreter. The calendar checks each row on day 1 of each month, for
   each faction with a ruler; a row fires once in a game. Included by
   sim.cpp after pk.cpp.

   The triggers and effects are documented. The clean room leaves out the
   choices of a player, the pictures, the absences of the chained events
   and the faction split of 連環計; the endings use the rules of events.md
   with the country name 漢 for an abdication (Q-189). */

/* ---- the clause language ------------------------------------------------------------- */

#define SIM_HISTORY_PARTS 6

static_assert(CONTENT_FORCE_CAPACITY == SIM_FORCE_CAPACITY, "the church table has one entry for each faction");

/* One clause, split at ':' into its parts. */
struct sim_history_clause
{
    char  text[128];
    char *part[SIM_HISTORY_PARTS];
    u32   count;
};

/* Copies the clause that starts at text and moves text past its ';'. */
internal b32
sim_history_next(const char **text, sim_history_clause *clause)
{
    const char *at = *text;
    if (!*at || strcmp(at, "-") == 0) return false;
    u32 size = 0;
    while (*at && *at != ';' && size < sizeof(clause->text) - 1) clause->text[size++] = *at++;
    clause->text[size] = 0;
    if (*at == ';') ++at;
    *text = at;
    clause->count = 0;
    char *part = clause->text;
    while (clause->count < SIM_HISTORY_PARTS)
    {
        clause->part[clause->count++] = part;
        char *colon = strchr(part, ':');
        if (!colon) break;
        *colon = 0;
        part = colon + 1;
    }
    for (u32 index = clause->count; index < SIM_HISTORY_PARTS; ++index) clause->part[index] = (char *)"";
    return true;
}

internal u32
sim_history_number(const char *text)
{
    b32 negative = *text == '-';
    if (negative) ++text;
    u32 value = 0;
    while (*text >= '0' && *text <= '9') value = value * 10 + (u32)(*text++ - '0');
    return negative ? (u32)-(s32)value : value;
}

/* "194/12": the months since year 0. */
internal u32
sim_history_stamp(const char *text)
{
    u32 year = sim_history_number(text);
    const char *slash = strchr(text, '/');
    return year * 12 + (slash ? sim_history_number(slash + 1) : 1);
}

/* The officer of the row with these Big5 bytes, or 0. */
internal officer_component *
sim_officer_by_name(sim_state *sim, const u8 *name, u32 size)
{
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        u8 full[OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE];
        u32 used = 0;
        for (u32 at = 0; at < OFFICER_FAMILY_NAME_SIZE && officer->family_name[at]; ++at) full[used++] = officer->family_name[at];
        for (u32 at = 0; at < OFFICER_GIVEN_NAME_SIZE && officer->given_name[at]; ++at) full[used++] = officer->given_name[at];
        if (used == size && memcmp(full, name, size) == 0) return officer;
    }
    return 0;
}

/* "@2": the officer 2 of the row. */
internal officer_component *
sim_history_officer(sim_state *sim, history_kind *kind, const char *text)
{
    if (text[0] != '@') return 0;
    u32 index = sim_history_number(text + 1);
    if (index >= kind->name_count) return 0;
    return sim_officer_by_name(sim, kind->names[index], kind->name_sizes[index]);
}

/* Calls each item of a '|' list: returns how many items the test accepts and the item count. */
struct sim_history_list
{
    char  text[128];
    char *item[16];
    u32   count;
};

internal void
sim_history_split_list(const char *text, sim_history_list *list)
{
    u32 size = 0;
    while (text[size] && size < sizeof(list->text) - 1) { list->text[size] = text[size]; ++size; }
    list->text[size] = 0;
    list->count = 0;
    char *item = list->text;
    while (list->count < ArrayCount(list->item))
    {
        list->item[list->count++] = item;
        char *bar = strchr(item, '|');
        if (!bar) break;
        *bar = 0;
        item = bar + 1;
    }
}

internal b32
sim_officer_free_to_join(sim_state *sim, officer_component *officer)
{
    return officer && officer->status == OFFICER_STATUS_FREE_OR_NOT_APPEARED && officer->appearance_year <= sim->turns.date.year;
}

internal b32
sim_officer_dead(officer_component *officer)
{
    return !officer || officer->status == OFFICER_STATUS_DEAD || officer->status == OFFICER_STATUS_EMPTY_SLOT;
}

internal u32
sim_faction_officer_count(sim_state *sim, u8 force)
{
    u32 count = 0;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        count += sim_officer_is_active(officer) && officer->force_id == force;
    }
    return count;
}

internal city_component *
sim_officer_city(sim_state *sim, officer_component *officer)
{
    return officer && officer->current_position < SAVE_CITY_COUNT ? sim_find_city(sim, (u8)officer->current_position) : 0;
}

/* ---- the conditions ---------------------------------------------------------------------- */

/* owner: true if the owner of the city is in the list of force ids. A dash stands for no owner. */
internal b32
sim_history_owner_in(sim_state *sim, u8 city_id, const char *text)
{
    city_component *city = sim_find_city(sim, city_id);
    if (!city) return false;
    sim_history_list list;
    sim_history_split_list(text, &list);
    for (u32 index = 0; index < list.count; ++index)
    {
        u8 listed = strcmp(list.item[index], "-") == 0 ? (u8)SIM_NO_FORCE : (u8)sim_history_number(list.item[index]);
        if (city->owner_force_id == listed) return true;
    }
    return false;
}

/* church: true if the church of the faction is in the list of names. The engine fills the churches. */
internal b32
sim_history_church_in(sim_state *sim, u8 force, const char *text)
{
    if (force >= CONTENT_FORCE_CAPACITY || sim->content.force_church[force] == CONTENT_CHURCH_NONE) return false;
    sim_history_list list;
    sim_history_split_list(text, &list);
    for (u32 index = 0; index < list.count; ++index)
    {
        if (content_find_church(list.item[index]) == sim->content.force_church[force]) return true;
    }
    return false;
}

/* The row of the table with this key, or the row count. */
internal u32
sim_history_row_of(sim_state *sim, const char *key)
{
    for (u32 row = 0; row < sim->content.history_count; ++row)
    {
        if (strcmp(sim->content.history[row].key, key) == 0) return row;
    }
    return sim->content.history_count;
}

/* after: true if the row with this key has started. */
internal b32
sim_history_started(sim_state *sim, const char *key)
{
    u32 row = sim_history_row_of(sim, key);
    return row < sim->content.history_count && sim->history_turn[row] != 0;
}

internal b32
sim_history_condition(sim_state *sim, history_kind *kind, sim_history_clause *clause, u8 force)
{
    const char *name = clause->part[0];
    officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, force));
    u32 date  = (u32)sim->turns.date.year * 12 + sim->turns.date.month;
    u32 start = (u32)sim->turns.start_date.year * 12 + sim->turns.start_date.month;
    u32 days  = sim_days_since_start(sim);
    officer_component *officer = sim_history_officer(sim, kind, clause->part[1]);
    u32 value = sim_history_number(clause->part[1]);
    u32 second = sim_history_number(clause->part[2]);
    if (strcmp(name, "scenario") == 0)     return sim->scenario_number != SIM_NO_SCENARIO && sim->scenario_number + 1u == value;
    if (strcmp(name, "opening") == 0)      return days < 30;
    if (strcmp(name, "start_before") == 0) return start <  sim_history_stamp(clause->part[1]);
    if (strcmp(name, "start_by") == 0)     return start <= sim_history_stamp(clause->part[1]);
    if (strcmp(name, "start_from") == 0)   return start >= sim_history_stamp(clause->part[1]);
    if (strcmp(name, "date_from") == 0)    return date  >= sim_history_stamp(clause->part[1]);
    if (strcmp(name, "date_until") == 0)   return date  <= sim_history_stamp(clause->part[1]);
    if (strcmp(name, "at_war") == 0)       return sim_at_war(sim, (u8)value, (u8)second);
    if (strcmp(name, "allied") == 0)       return value != second && sim_relation(sim, (u8)value, (u8)second) == RELATION_ALLIANCE;
    if (strcmp(name, "owner") == 0)        return sim_history_owner_in(sim, (u8)value, clause->part[2]);
    if (strcmp(name, "church") == 0)       return sim_history_church_in(sim, force, clause->part[1]);
    if (strcmp(name, "after") == 0)        return sim_history_started(sim, clause->part[1]);
    if (strcmp(name, "days") == 0)         return days >= value;
    if (strcmp(name, "ruler") == 0)        return officer && officer == ruler;
    if (strcmp(name, "faction_of") == 0)   return officer && sim_officer_is_active(officer) && officer->force_id == force;
    if (strcmp(name, "ruler_family") == 0) return officer && ruler && memcmp(officer->family_name, ruler->family_name, OFFICER_FAMILY_NAME_SIZE) == 0;
    if (strcmp(name, "rules") == 0 || strcmp(name, "rules_any") == 0 || strcmp(name, "not_rules") == 0)
    {
        sim_history_list list;
        sim_history_split_list(clause->part[1], &list);
        u32 owned = 0;
        for (u32 index = 0; index < list.count; ++index)
        {
            city_component *city = sim_find_city(sim, (u8)sim_history_number(list.item[index]));
            owned += city && city->owner_force_id == force;
        }
        if (name[0] == 'n') return owned == 0;
        return strcmp(name, "rules") == 0 ? owned == list.count : owned > 0;
    }
    if (strcmp(name, "cities") == 0)         return sim_faction_city_count(sim, force) >= value;
    if (strcmp(name, "cities_at_most") == 0) return sim_faction_city_count(sim, force) <= value;
    if (strcmp(name, "rank") == 0)           return sim->rank[force] >= value;
    if (strcmp(name, "rank_below") == 0)     return sim->rank[force] < value;
    if (strcmp(name, "free") == 0 || strcmp(name, "free_any") == 0 || strcmp(name, "unmarried") == 0)
    {
        sim_history_list list;
        sim_history_split_list(clause->part[1], &list);
        u32 hits = 0;
        for (u32 index = 0; index < list.count; ++index)
        {
            officer_component *item = sim_history_officer(sim, kind, list.item[index]);
            if (name[0] == 'u') hits += item && !sim_officer_dead(item) && item->spouse == SIM_NO_OFFICER;
            else                hits += sim_officer_free_to_join(sim, item);
        }
        return strcmp(name, "free_any") == 0 ? hits > 0 : hits == list.count;
    }
    if (strcmp(name, "serves") == 0)         return officer && sim_officer_is_active(officer) && officer->force_id == force && officer != ruler && officer->loyalty >= second;
    if (strcmp(name, "has") == 0)            return officer && sim_officer_is_active(officer) && officer->force_id == force;
    if (strcmp(name, "captive") == 0)        return officer && officer->status == SIM_STATUS_CAPTIVE && officer->force_id == force;
    if (strcmp(name, "in_other") == 0)       return officer && sim_officer_is_active(officer) && officer->force_id != force && officer->force_id != SIM_NO_FORCE && officer->status != OFFICER_STATUS_RULER;
    if (strcmp(name, "loyalty_below") == 0)  return officer && officer->loyalty < second;
    if (strcmp(name, "merit_at_least") == 0) return officer && officer->merit >= second;
    if (strcmp(name, "order_ruler") == 0 || strcmp(name, "order_ruler_below") == 0)
    {
        city_component *city = sim_officer_city(sim, ruler);
        if (!city) return false;
        return name[11] == 0 ? city->public_order >= value : city->public_order < value;
    }
    if (strcmp(name, "order_of") == 0)       { city_component *city = sim_officer_city(sim, officer); return city && city->public_order >= second; }
    if (strcmp(name, "emperor_held") == 0)   return sim_emperor_protector(sim) == force;
    if (strcmp(name, "emperor_is") == 0)     return sim->emperor.who == value;
    if (strcmp(name, "no_emperor") == 0)     return sim->emperor.who == EMPEROR_NONE;
    if (strcmp(name, "dead") == 0)           return sim_officer_dead(officer);
    if (strcmp(name, "death_year") == 0)     return officer && !sim_officer_dead(officer) && officer->death_year && sim->turns.date.year >= officer->death_year;
    if (strcmp(name, "adviser") == 0)        return officer && sim->adviser[force] == officer->officer_id;
    if (strcmp(name, "officers") == 0)       return sim_faction_officer_count(sim, force) >= value;
    if (strcmp(name, "age") == 0)            return officer && sim_officer_age(sim, officer) >= second;
    if (strcmp(name, "faction_cities") == 0)
    {
        return officer && sim_officer_is_active(officer) && officer->force_id != force && officer->force_id != SIM_NO_FORCE &&
               sim_faction_city_count(sim, officer->force_id) >= second;
    }
    return false;
}

/* True if every condition of the row holds for the faction. */
internal b32
sim_history_ready(sim_state *sim, u32 row, u8 force)
{
    if (row >= sim->content.history_count || force >= SIM_FORCE_CAPACITY || sim_force_ruler(sim, force) == SIM_NO_OFFICER) return false;
    history_kind *kind = sim->content.history + row;
    const char *text = kind->conditions;
    sim_history_clause clause;
    while (sim_history_next(&text, &clause))
    {
        if (!sim_history_condition(sim, kind, &clause, force)) return false;
    }
    return true;
}

/* ---- the effects ------------------------------------------------------------------------- */

/* The city where a joining officer goes: the city of the ruler, or the first city of the faction. */
internal u8
sim_history_home(sim_state *sim, u8 force)
{
    city_component *city = sim_officer_city(sim, sim_find_officer(sim, sim_force_ruler(sim, force)));
    if (city && city->owner_force_id == force) return city->city_id;
    for (u32 index = 0; index < sim->cities.count; ++index) if (sim->cities.data[index].owner_force_id == force) return sim->cities.data[index].city_id;
    return SIM_NO_CITY;
}

internal void
sim_history_join(sim_state *sim, officer_component *officer, u8 force, u32 loyalty, u32 merit, u32 cause, sim_event_sink *events)
{
    u8 home = sim_history_home(sim, force);
    if (!officer || home == SIM_NO_CITY) return;
    sim_join_faction(sim, officer, force, home, cause, events);
    officer->loyalty = (u8)Minimum(loyalty, 100u);
    officer->merit   = (u16)Minimum(Maximum((u32)officer->merit, merit), 65535u);
}

internal void
sim_history_effect(sim_state *sim, history_kind *kind, sim_history_clause *clause, u8 force, u32 cause, sim_event_sink *events)
{
    const char *name = clause->part[0];
    officer_component *officer = sim_history_officer(sim, kind, clause->part[1]);
    officer_component *other   = sim_history_officer(sim, kind, clause->part[2]);
    s32 value  = (s32)sim_history_number(clause->part[1]);
    s32 second = (s32)sim_history_number(clause->part[2]);
    u32 third  = sim_history_number(clause->part[3]);
    if (strcmp(name, "join") == 0)
    {
        b32 joins = sim_officer_free_to_join(sim, officer) || (officer && officer->status == SIM_STATUS_CAPTIVE && officer->force_id == force);
        if (joins) sim_history_join(sim, officer, force, (u32)second, third, cause, events);
        return;
    }
    if (strcmp(name, "defect") == 0)
    {
        if (officer && sim_officer_is_active(officer) && officer->force_id != force && officer->status != OFFICER_STATUS_RULER) sim_history_join(sim, officer, force, (u32)second, third, cause, events);
        return;
    }
    if (strcmp(name, "merit") == 0 && officer)
    {
        officer->merit = (u16)Minimum((u32)officer->merit + (u32)second, 65535u);
        sim_emit_officer(sim, SIM_EVENT_MERIT_GAINED, officer, second, 0, false, cause, events);
        return;
    }
    if (strcmp(name, "merit_like") == 0 && officer && other)
    {
        if (other->merit > officer->merit) officer->merit = other->merit;
        return;
    }
    if (strcmp(name, "loyalty") == 0 && officer)
    {
        sim_change_loyalty(sim, officer, second - (s32)officer->loyalty, cause, events);
        return;
    }
    if (strcmp(name, "loyalty_all") == 0)
    {
        for (u32 index = 0; index < sim->officers.count; ++index)
        {
            officer_component *each = sim->officers.data + index;
            if (sim_officer_is_active(each) && each->force_id == force && each->status != OFFICER_STATUS_RULER) sim_change_loyalty(sim, each, value, cause, events);
        }
        return;
    }
    if (strcmp(name, "order_all") == 0 || strcmp(name, "morale_all") == 0)
    {
        b32 order = name[0] == 'o';
        for (u32 index = 0; index < sim->cities.count; ++index)
        {
            city_component *city = sim->cities.data + index;
            if (city->owner_force_id != force) continue;
            /* A negative value lowers the number, and the number stays between 0 and its limit (E7). */
            s32 limit = (s32)(sim->content.rules.morale_max ? sim->content.rules.morale_max : 100u);
            if (order) city->public_order = (u8)Minimum(Maximum((s32)city->public_order + value, 0), 100);
            else       city->morale       = (u8)Minimum(Maximum((s32)city->morale + value, 0), limit);
        }
        return;
    }
    if (strcmp(name, "marry") == 0 && officer && other && !sim_officer_dead(officer) && !sim_officer_dead(other))
    {
        officer->spouse = other->officer_id;
        other->spouse   = officer->officer_id;
        sim_emit_officer(sim, SIM_EVENT_MARRIED, officer, 0, other->officer_id, false, cause, events);
        return;
    }
    if (strcmp(name, "sworn") == 0)
    {
        sim_history_list list;
        sim_history_split_list(clause->part[1], &list);
        officer_component *head = sim_history_officer(sim, kind, list.item[0]);
        for (u32 index = 0; index < list.count && head; ++index)
        {
            officer_component *each = sim_history_officer(sim, kind, list.item[index]);
            if (!each) continue;
            each->sworn_brother_group_head = head->officer_id;
            if (index) sim_emit_officer(sim, SIM_EVENT_SWORN, each, 0, head->officer_id, false, cause, events);
        }
        return;
    }
    if (strcmp(name, "dies") == 0 && officer && !sim_officer_dead(officer))
    {
        sim_officer_dies(sim, officer, cause, events);
        return;
    }
    if (strcmp(name, "rank") == 0)
    {
        sim->rank[force] = (u8)Minimum((u32)value, sim->content.rank_count ? sim->content.rank_count - 1 : 0u);
        return;
    }
    if (strcmp(name, "emperor") == 0)
    {
        sim_set_emperor(sim, (u8)Minimum((u32)value, (u32)EMPEROR_KIND_COUNT - 1), cause, events);
        return;
    }
    if (strcmp(name, "friendliness_all") == 0)
    {
        for (u32 other_force = 0; other_force < SIM_DIPLOMACY_FORCES; ++other_force)
        {
            if (other_force == force || sim_force_ruler(sim, (u8)other_force) == SIM_NO_OFFICER) continue;
            sim_change_friendliness(sim, force, (u8)other_force, value, cause, events);
            if (value < 0) sim->diplomacy.relation[force][other_force] = sim->diplomacy.relation[other_force][force] = RELATION_NONE;
        }
        return;
    }
    if (strcmp(name, "friendliness_with") == 0 && officer && officer->force_id < SIM_DIPLOMACY_FORCES)
    {
        sim_change_friendliness(sim, force, officer->force_id, second, cause, events);
        if (second < 0) sim->diplomacy.relation[force][officer->force_id] = sim->diplomacy.relation[officer->force_id][force] = RELATION_NONE;
        return;
    }
    if (strcmp(name, "raise") == 0 && officer)
    {
        u32 ability = content_find_name(content_ability_names, CONTENT_ABILITY_COUNT, clause->part[2]);
        u8 *field = sim_ability_field(officer, ability);
        if (field && *field < third) sim_change_ability(sim, officer, ability, (s32)third - (s32)*field, false, cause, events);
        return;
    }
    if (strcmp(name, "adviser") == 0 && officer && sim_officer_is_active(officer) && officer->force_id == force)
    {
        sim->adviser[force] = officer->officer_id;
        return;
    }
    if (strcmp(name, "seize") == 0)
    {
        officer_component *old_ruler = sim_find_officer(sim, sim_force_ruler(sim, force));
        b32 seizes = officer && old_ruler && officer != old_ruler && sim_officer_is_active(officer) && officer->force_id == force &&
                     officer->status != OFFICER_STATUS_RULER;
        if (!seizes) return;
        sim_clear_post(sim, old_ruler);
        old_ruler->status = OFFICER_STATUS_REGULAR_OFFICER;
        sim_clear_post(sim, officer);
        officer->status  = OFFICER_STATUS_RULER;
        officer->loyalty = 100;
        sim_emit_officer(sim, SIM_EVENT_RULER_SUCCEEDED, officer, 0, 0, false, cause, events);
        return;
    }
    if (strcmp(name, "city_loss") == 0)
    {
        city_component *city = sim_find_city(sim, (u8)value);
        if (!city) return;
        u64 kept = 100u - Minimum((u32)second, 100u);
        city->troops     = (u32)((u64)city->troops * kept / 100);
        city->durability = (u32)((u64)city->durability * kept / 100);
        return;
    }
    /* rename and target have no effect at the time of the row. The queries below read the rows that started. */
}

/* ---- the effects that last: a new name and a target ------------------------------------------- */

/* The faction of the officer of a clause, or SIM_NO_FORCE. The faction stays after the officer dies. */
internal u8
sim_history_clause_force(sim_state *sim, history_kind *kind, sim_history_clause *clause)
{
    officer_component *officer = sim_history_officer(sim, kind, clause->part[1]);
    return officer ? officer->force_id : (u8)SIM_NO_FORCE;
}

/* Gives the key of the new name that a row of history.tsv has set for the faction. The latest row
   wins. The sim has no state for the name: the rows that started and the officers give it. */
internal b32
sim_force_rename_key(sim_state *sim, u8 force, char *key, u32 size)
{
    b32 found = false;
    for (u32 row = 0; row < sim->content.history_count; ++row)
    {
        if (!sim->history_turn[row]) continue;
        history_kind *kind = sim->content.history + row;
        const char *text = kind->effects;
        sim_history_clause clause;
        while (sim_history_next(&text, &clause))
        {
            if (strcmp(clause.part[0], "rename") != 0 || sim_history_clause_force(sim, kind, &clause) != force) continue;
            u32 length = (u32)strlen(clause.part[2]);
            if (length == 0 || length + 1 > size) return false;
            memcpy(key, clause.part[2], length + 1);
            found = true;
        }
    }
    return found;
}

/* The city that a row has set as the target of the faction, or SIM_NO_CITY. The target ends after
   its months or when its officer dies. The AI of the faction attacks this city first. */
internal u8
sim_history_target(sim_state *sim, u8 force)
{
    u8 target = SIM_NO_CITY;
    for (u32 row = 0; row < sim->content.history_count; ++row)
    {
        if (!sim->history_turn[row]) continue;
        history_kind *kind = sim->content.history + row;
        const char *text = kind->effects;
        sim_history_clause clause;
        while (sim_history_next(&text, &clause))
        {
            if (strcmp(clause.part[0], "target") != 0 || sim_history_clause_force(sim, kind, &clause) != force) continue;
            if (sim_officer_dead(sim_history_officer(sim, kind, clause.part[1]))) continue;
            u32 started = sim->history_turn[row] - 1;
            u32 limit   = sim_history_number(clause.part[3]) * CALENDAR_TURNS_PER_MONTH;
            if (sim->turn >= started && sim->turn - started < limit) target = (u8)sim_history_number(clause.part[2]);
        }
    }
    return target;
}

internal void
sim_fire_history(sim_state *sim, u32 row, u8 force, u32 cause, sim_event_sink *events)
{
    history_kind *kind = sim->content.history + row;
    sim->history_turn[row] = sim->turn + 1;
    sim_event event = sim_event_make(sim, SIM_EVENT_HISTORY_FIRED, cause);
    event.faction = force;
    event.detail  = row;
    sim_emit(events, event);
    const char *text = kind->effects;
    sim_history_clause clause;
    while (sim_history_next(&text, &clause)) sim_history_effect(sim, kind, &clause, force, cause, events);
}

/* Day 1 of each month: each row that has not fired, for each faction with
   a ruler in turn order; the first faction that meets the conditions gets
   the event if the chance allows it. */
internal void
sim_run_history_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    if (!sim->settings.events || !sim->has_content) return;
    for (u32 row = 0; row < sim->content.history_count; ++row)
    {
        if (sim->history_turn[row]) continue;
        for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
        {
            if (!sim_history_ready(sim, row, (u8)force)) continue;
            u32 chance = sim->content.history[row].chance;
            if (chance >= 100 || sim_random_below(sim, 100) < chance) sim_fire_history(sim, row, (u8)force, cause, events);
            break;
        }
    }
}

internal sim_check
sim_check_trigger_history(sim_state *sim, sim_m8_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (!sim->has_content || order->value >= sim->content.history_count) return sim_refuse(check, SIM_REFUSAL_NO_EVENT);
    if (order->force >= SIM_FORCE_CAPACITY || sim_force_ruler(sim, order->force) == SIM_NO_OFFICER) return sim_refuse(check, SIM_REFUSAL_NO_FACTION);
    if (sim_busy(sim)) return sim_refuse(check, SIM_REFUSAL_BUSY);
    return check;
}

/* SIM_COMMAND_TRIGGER_HISTORY: the effects of the row now, without its conditions. */
internal void
sim_trigger_history_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    if (sim_check_trigger_history(sim, &command.m8).refusal != SIM_REFUSAL_NONE) return;
    sim_fire_history(sim, command.m8.value, command.m8.force, command.id, events);
}

/* ---- the endings ------------------------------------------------------------------------- */

/* The ending of a unification (events.md, "Endings"):
   - a ruler who holds the emperor and respects the Han, or a normal ruler
     with more than 15 cities at public order 100, receives the throne:
     the country stays 漢 and the ending is A;
   - else the ruler takes the title: B if the military facilities are more
     than 30 % of the built facilities, or the soldiers exceed 500000 and
     more than half of the officers are warriors; else C. */
internal u8
sim_ending_for(sim_state *sim, u8 force)
{
    officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, force));
    u32 calm_cities = 0, soldiers = 0;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id != force) continue;
        calm_cities += city->public_order >= 100;
        soldiers    += city->troops;
    }
    b32 protects = sim_emperor_protector(sim) == force;
    b32 abdication = protects && ruler && (ruler->han_respect == 2 || (ruler->han_respect == 1 && calm_cities > 15));
    if (abdication) return ENDING_HAN;
    u32 built = 0, military = 0;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id != force) continue;
        sim_city_site *site = sim->sites + city->city_id;
        for (u32 slot = 0; slot < site->slot_count; ++slot)
        {
            sim_slot *each = site->slots + slot;
            if (each->state != SIM_SLOT_BUILT || each->facility >= sim->content.facility_count) continue;
            built++;
            facility_effect effect = (facility_effect)sim->content.facilities[each->facility].effect;
            military += effect == FACILITY_EFFECT_CONSCRIPTION || effect == FACILITY_EFFECT_WEAPONS || effect == FACILITY_EFFECT_HORSES ||
                        effect == FACILITY_EFFECT_SIEGE || effect == FACILITY_EFFECT_SHIPS;
        }
    }
    u32 officers = 0, warriors = 0;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!sim_officer_is_active(officer) || officer->force_id != force) continue;
        officers++;
        warriors += (u32)officer->war + officer->leadership >= (u32)officer->intelligence + officer->politics;
    }
    if ((built && military * 100 > built * 30) || (soldiers > 500000 && warriors * 2 > officers)) return ENDING_CONQUEST;
    return ENDING_PROSPERITY;
}

/* ---- the portraits ----------------------------------------------------------------------- */

/* 1 when the officer has the age of its second portrait (portraits.tsv), else 0. */
internal u8
sim_portrait_stage(sim_state *sim, officer_component *officer)
{
    if (!officer) return 0;
    u8 full[OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE];
    u32 used = 0;
    for (u32 at = 0; at < OFFICER_FAMILY_NAME_SIZE && officer->family_name[at]; ++at) full[used++] = officer->family_name[at];
    for (u32 at = 0; at < OFFICER_GIVEN_NAME_SIZE && officer->given_name[at]; ++at) full[used++] = officer->given_name[at];
    for (u32 row = 0; row < sim->content.portrait_count; ++row)
    {
        portrait_kind *kind = sim->content.portraits + row;
        if (kind->name_size == used && memcmp(kind->name, full, used) == 0) return sim_officer_age(sim, officer) >= kind->age ? 1 : 0;
    }
    return 0;
}
