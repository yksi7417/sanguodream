/* M18: the facts of a ledger event for a text provider (ROADMAP M18).
   The player decided on 2026-10-20 (spec DECISIONS.md) that a plug-in of
   the front end writes the words of each message. The first plug-in uses
   templates. A later one can use a small language model. Thus the engine
   gives each event its facts: the kind, the subject, the amount, the
   speaker and the personality of the speaker, the tone and the sign.
   The pack table event_tones.tsv gives the tone, the sign rule and the
   report flag of each kind. The spec has no list of tones. The rows of the
   pack are assumed, and a pack without the table works with defaults. */

enum engine_sign_rule
{
    ENGINE_SIGN_RULE_AMOUNT,    /* good if the amount rises, bad if it falls */
    ENGINE_SIGN_RULE_GOOD,
    ENGINE_SIGN_RULE_BAD,
    ENGINE_SIGN_RULE_NEUTRAL,
    ENGINE_SIGN_RULE_COUNT
};

#define ENGINE_DEFAULT_TONE "neutral"

/* Reads event_tones.tsv of the pack. A row names a kind by its key
   (sim_event_name). A row with an unknown key or an unknown sign is
   skipped. */
internal void
engine_load_event_tones(engine *e, const char *folder)
{
    static const char *signs[ENGINE_SIGN_RULE_COUNT] = { "amount", "good", "bad", "neutral" };
    for (u32 kind = 0; kind < SIM_EVENT_COUNT; ++kind) e->event_tones[kind] = {};
    tsv_table table = {};
    arena_reset(&e->scratch);
    if (!folder || !engine_read_table(e, folder, "event_tones.tsv", &table)) return;
    for (u32 row = 0; row < table.row_count; ++row)
    {
        const char *key = tsv_cell(&table, row, "key"), *tone = tsv_cell(&table, row, "tone");
        u32 sign = content_find_name(signs, ENGINE_SIGN_RULE_COUNT, tsv_cell(&table, row, "sign"));
        if (sign == ENGINE_SIGN_RULE_COUNT || !tone[0] || strlen(tone) >= ENGINE_TONE_SIZE) continue;
        for (u32 kind = 0; kind < SIM_EVENT_COUNT; ++kind)
        {
            if (strcmp(sim_event_name((sim_event_kind)kind), key) != 0) continue;
            engine_event_tone *entry = e->event_tones + kind;
            memcpy(entry->tone, tone, strlen(tone) + 1);
            entry->sign_rule = (u8)sign;
            entry->report    = strcmp(tsv_cell(&table, row, "report"), "yes") == 0;
            entry->hidden    = strcmp(tsv_cell(&table, row, "log"), "no") == 0;
            entry->assumed   = strcmp(tsv_cell(&table, row, "status"), "documented") != 0;
            entry->present   = true;
            break;
        }
    }
    arena_reset(&e->scratch);
}

/* The first officer of a command: the officer who speaks for the events of
   the command. Commands without officers leave no entry. */
internal void
engine_note_speaker(engine *e, const sim_command *command)
{
    u16 officer = SIM_NO_OFFICER;
    switch (command->kind)
    {
        case SIM_COMMAND_DEVELOP:  officer = command->develop.officers[0];  break;
        case SIM_COMMAND_PATROL:
        case SIM_COMMAND_CONSCRIPT:
        case SIM_COMMAND_TRAIN:
        case SIM_COMMAND_MERCHANT:
        case SIM_COMMAND_PRODUCE:
        {
            /* G2-018, G3-014: one of the executors speaks the result. In
               seven observations slot 1 spoke five times and slot 3 two
               times, with two speakers for the same command, so the
               original picks at random. The clone picks by the command id,
               which keeps the replay deterministic. */
            u16 executors[SIM_COMMAND_OFFICERS];
            u32 count = 0;
            for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
            {
                if (command->city.officers[place] != SIM_NO_OFFICER) executors[count++] = command->city.officers[place];
            }
            officer = count ? executors[command->id % count] : (u16)SIM_NO_OFFICER;
        } break;
        case SIM_COMMAND_RESEARCH: officer = command->research.officers[0]; break;
        case SIM_COMMAND_FACTION_PLOT:
        case SIM_COMMAND_DIPLOMACY: officer = command->court.officer;        break;   /* the result comes later with this cause (travel.cpp) */
        case SIM_COMMAND_MOVE_OFFICERS:
        case SIM_COMMAND_SUMMON:   officer = command->move.officers[0];     break;
        default: break;
    }
    if (officer == SIM_NO_OFFICER || !command->id) return;
    u32 place = command->id % ENGINE_SPEAKER_RING;
    e->speaker_commands[place] = command->id;
    e->speaker_officers[place] = officer;
}

internal u16
engine_speaker_of_command(engine *e, u32 command_id)
{
    if (!command_id) return ENGINE_NO_OFFICER;
    u32 place = command_id % ENGINE_SPEAKER_RING;
    return e->speaker_commands[place] == command_id ? e->speaker_officers[place] : ENGINE_NO_OFFICER;
}

/* The date of a past turn: the date of today, that many turns back. */
internal engine_date
engine_date_of_turn(engine *e, u32 turn)
{
    engine_date today = engine_api_date(e);
    u32 back = e->sim->turn > turn ? e->sim->turn - turn : 0;
    s64 number = calendar_turn_number(e->sim->turns.date) - (s64)back;
    if (number < 0) number = 0;
    s64 month_number = number / CALENDAR_TURNS_PER_MONTH;
    calendar_date date = {};
    date.year          = (u16)(month_number / CALENDAR_MONTHS_PER_YEAR);
    date.month         = (u8)(month_number % CALENDAR_MONTHS_PER_YEAR + 1);
    date.turn_of_month = (u8)(number % CALENDAR_TURNS_PER_MONTH);
    engine_date result = today;
    result.year             = date.year;
    result.month            = date.month;
    result.day              = calendar_day(date);
    result.season           = (u8)calendar_season_of(date);
    result.days_since_start = calendar_days_between(e->sim->turns.start_date, date);
    return result;
}

/* The speaker of an event: the first officer of its command, the officer
   that it names, the governor of the city that it names, or the ruler of
   its faction. The spec does not name the speaker of each report (an open
   point of M17), so the clean room uses this order. */
internal u16
engine_speaker_of_event(engine *e, const sim_event *event, engine_subject subject)
{
    u16 speaker = engine_speaker_of_command(e, event->cause);
    if (speaker != ENGINE_NO_OFFICER) return speaker;
    if (subject.kind == ENGINE_SUBJECT_OFFICER) return subject.id;
    if (subject.kind == ENGINE_SUBJECT_CITY)
    {
        engine_city city = engine_api_city_at(e, subject.id);
        if (city.commanding_officer_id != 0xFFFF) return city.commanding_officer_id;
    }
    if (event->faction != SIM_NO_FORCE) return engine_api_force_ruler(e, event->faction);
    return ENGINE_NO_OFFICER;
}

internal u8
engine_sign_of(u8 rule, s32 delta)
{
    switch (rule)
    {
        case ENGINE_SIGN_RULE_GOOD:    return (u8)ENGINE_SIGN_GOOD;
        case ENGINE_SIGN_RULE_BAD:     return (u8)ENGINE_SIGN_BAD;
        case ENGINE_SIGN_RULE_NEUTRAL: return (u8)ENGINE_SIGN_NEUTRAL;
        default: return (u8)(delta > 0 ? ENGINE_SIGN_GOOD : delta < 0 ? ENGINE_SIGN_BAD : ENGINE_SIGN_NEUTRAL);
    }
}

/* The kinds that sim_emit_unit writes: the detail is the unit. */
internal b32
engine_detail_is_unit(sim_event_kind kind)
{
    switch (kind)
    {
        case SIM_EVENT_MORALE_CHANGED:
        case SIM_EVENT_UNIT_LOOT:
        case SIM_EVENT_UNIT_PLUNDER_GRAIN:
        case SIM_EVENT_UNIT_DESTROYED:
        case SIM_EVENT_UNIT_DAMAGED:
        case SIM_EVENT_CRITICAL_HIT:
        case SIM_EVENT_COUNTERATTACK:
        case SIM_EVENT_UNIT_PUSHED:
        case SIM_EVENT_TACTIC_USED:
        case SIM_EVENT_UNIT_STATE:
        case SIM_EVENT_PLOT_USED:
        case SIM_EVENT_UNIT_MOVED:
        case SIM_EVENT_UNIT_ORDERED:
        case SIM_EVENT_UNIT_ORDER_DONE:
        case SIM_EVENT_UNIT_SOLDIERS_LOST:
        case SIM_EVENT_UNIT_WAITED:
        case SIM_EVENT_UNIT_SUPPLIED:
        case SIM_EVENT_UNIT_GRAIN_EATEN:
        case SIM_EVENT_UNIT_DESERTED:
            return true;
        default:
            return false;
    }
}

internal engine_place
engine_place_on(sim_state *sim, engine_place place, hex_coord hex)
{
    place.on_map = hex_map_contains(&sim->map, hex);
    place.q      = hex.q;
    place.r      = hex.r;
    return place;
}

/* The place of an event (engine_place): the facility slot or the field
   building that it names, the fire cell, the port or the pass, the city of
   its subject, its unit, or the city or the unit of its officer. */
internal engine_place
engine_place_of_event(engine *e, const sim_event *event, engine_subject subject)
{
    sim_state *sim = e->sim;
    engine_place place = {};
    place.thing   = ENGINE_NO_ROW;
    place.force   = ENGINE_NO_FACTION;
    place.officer = ENGINE_NO_OFFICER;
    switch (event->kind)
    {
        case SIM_EVENT_FACILITY_REPAIRED:
        case SIM_EVENT_FACILITY_DAMAGED:
        case SIM_EVENT_FACILITY_DESTROYED:
        case SIM_EVENT_FACILITY_MERGED:
        {
            if (subject.kind != ENGINE_SUBJECT_CITY || subject.id >= SAVE_CITY_COUNT || event->detail >= sim->sites[subject.id].slot_count) break;
            sim_slot *slot = sim->sites[subject.id].slots + event->detail;
            u8 facility = event->kind == SIM_EVENT_FACILITY_DESTROYED ? (u8)event->delta : slot->facility;   /* the slot of a destroyed facility is empty */
            place.kind  = ENGINE_PLACE_FACILITY;
            place.id    = subject.id;
            place.slot  = (u8)event->detail;
            place.thing = facility == CONTENT_NO_FACILITY ? (u8)ENGINE_NO_FACILITY : facility;
            return engine_place_on(sim, place, slot->hex);
        }
        case SIM_EVENT_BUILDING_STARTED:
        case SIM_EVENT_BUILDING_COMPLETED:
        case SIM_EVENT_BUILDING_DAMAGED:
        case SIM_EVENT_BUILDING_DESTROYED:
        case SIM_EVENT_TRAP_TRIGGERED:
        case SIM_EVENT_ROCKFALL_RETURNED:
        {
            if (event->detail >= sim->building_count) break;
            sim_building *building = sim->buildings + event->detail;
            place.kind  = ENGINE_PLACE_BUILDING;
            place.id    = event->detail;
            place.thing = building->kind;
            place.force = building->force_id == SIM_NO_FORCE ? (u8)ENGINE_NO_FACTION : building->force_id;
            return engine_place_on(sim, place, building->hex);
        }
        case SIM_EVENT_FIRE_STARTED:
        case SIM_EVENT_FIRE_ENDED:
        {
            if (event->detail >= sim->map.width * sim->map.height) break;
            place.kind = ENGINE_PLACE_HEX;
            place.id   = event->detail;
            return engine_place_on(sim, place, hex_map_coord(&sim->map, event->detail));
        }
        case SIM_EVENT_BASE_TAKEN:
        case SIM_EVENT_UNIT_ENTERED_BASE:
        {
            if (event->detail < CONTENT_FIRST_BASE || event->detail >= CONTENT_FIRST_BASE + CONTENT_BASE_COUNT) break;
            place.kind = ENGINE_PLACE_CITY;
            place.id   = event->detail;
            return engine_place_on(sim, place, sim_travel_base_hex(sim, (u8)event->detail));
        }
        default: break;
    }
    if (subject.kind == ENGINE_SUBJECT_CITY)
    {
        place.kind = ENGINE_PLACE_CITY;
        place.id   = subject.id;
        return engine_place_on(sim, place, sim_travel_base_hex(sim, (u8)subject.id));
    }
    u32 unit = SIM_NO_UNIT;
    if (engine_detail_is_unit(event->kind)) unit = event->detail;
    if (subject.kind == ENGINE_SUBJECT_OFFICER)
    {
        engine_officer officer = engine_api_officer_by_id(e, subject.id);
        if (officer.officer_id != ENGINE_NO_OFFICER) place.officer = subject.id;
        if (officer.officer_id == ENGINE_NO_OFFICER) officer.current_position = 0xFFFF;
        if (unit == SIM_NO_UNIT && officer.current_position < CONTENT_FIRST_BASE + CONTENT_BASE_COUNT)
        {
            place.kind = ENGINE_PLACE_CITY;
            place.id   = officer.current_position;
            return engine_place_on(sim, place, sim_travel_base_hex(sim, (u8)officer.current_position));
        }
        if (unit == SIM_NO_UNIT && officer.current_position != 0xFFFF) unit = officer.current_position - (CONTENT_FIRST_BASE + CONTENT_BASE_COUNT);   /* 87 + the unit index */
    }
    if (unit >= sim->unit_count) return place;
    place.kind  = ENGINE_PLACE_UNIT;
    place.id    = unit;
    place.force = sim->units[unit].force_id == SIM_NO_FORCE ? (u8)ENGINE_NO_FACTION : sim->units[unit].force_id;
    if (place.officer == ENGINE_NO_OFFICER && sim->units[unit].officers[0] != SIM_NO_OFFICER) place.officer = sim->units[unit].officers[0];
    return engine_place_on(sim, place, sim->units[unit].hex);   /* a destroyed unit keeps the hex where it fell */
}

internal engine_event_report
engine_api_event_report(void *opaque, u32 ledger_index)
{
    engine *e = (engine *)opaque;
    engine_event_report report = {};
    report.ledger_index = ENGINE_NO_EVENT;
    report.speaker      = ENGINE_NO_OFFICER;
    report.faction      = ENGINE_NO_FACTION;
    memcpy(report.tone, ENGINE_DEFAULT_TONE, sizeof(ENGINE_DEFAULT_TONE));
    if (ledger_index >= e->ledger_count) return report;
    sim_event *event = e->ledger + ledger_index;
    engine_event_tone *tone = event->kind < SIM_EVENT_COUNT ? e->event_tones + event->kind : 0;
    report.ledger_index = ledger_index;
    report.kind         = (u8)event->kind;
    report.key          = sim_event_name(event->kind);
    report.resource     = (u8)sim_event_resource(event->kind);
    report.resource_key = sim_resource_name(sim_event_resource(event->kind));
    report.turn         = event->turn;
    report.date         = engine_date_of_turn(e, event->turn);
    report.subject      = engine_api_subject_of(e, event->subject);
    report.faction      = event->faction == SIM_NO_FORCE ? ENGINE_NO_FACTION : event->faction;
    report.delta        = event->delta;
    report.detail       = event->detail;
    report.cause        = event->cause;
    report.speaker      = engine_speaker_of_event(e, event, report.subject);
    if (report.speaker != ENGINE_NO_OFFICER) report.personality = engine_api_officer_detail(e, report.speaker).personality;
    if (tone && tone->present) memcpy(report.tone, tone->tone, sizeof(report.tone));
    report.sign    = engine_sign_of(tone && tone->present ? tone->sign_rule : (u8)ENGINE_SIGN_RULE_AMOUNT, event->delta);
    report.report  = tone && tone->present && tone->report;
    report.log     = !(tone && tone->present && tone->hidden);
    report.assumed = event->assumed || !tone || !tone->present || tone->assumed;
    report.place   = engine_place_of_event(e, event, report.subject);
    return report;
}
