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
        case SIM_COMMAND_PRODUCE:  officer = command->city.officers[0];     break;
        case SIM_COMMAND_RESEARCH: officer = command->research.officers[0]; break;
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
    report.assumed = event->assumed || !tone || !tone->present || tone->assumed;
    return report;
}
