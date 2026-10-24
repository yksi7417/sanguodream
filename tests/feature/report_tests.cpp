/* M18 feature tests: the facts of the ledger events for a text provider
   (engine_report.cpp, event_report). The sample save with the real engine
   and the placeholder pack, so that event_tones.tsv is the pack table. */

/* The index of the last ledger event of a kind, or ENGINE_NO_EVENT. */
internal u32
feature_last_event(engine_api *api, sim_event_kind kind)
{
    for (u32 index = api->ledger_count(api->engine); index > 0; --index)
    {
        if (api->ledger_at(api->engine, index - 1).kind == kind) return index - 1;
    }
    return ENGINE_NO_EVENT;
}

internal void
feature_report_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city;

    u16 ids[SIM_DEVELOP_OFFICERS];
    feature_take_officers(api, city, ids, 1);
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 50);
    TEST_CHECK(counts, api->command_patrol(e, city, ids).refusal == SIM_REFUSAL_NONE, "report: a patrol runs");
    u32 patrol = feature_last_event(api, SIM_EVENT_PATROLLED);
    engine_event_report report = api->event_report(e, patrol);
    TEST_CHECK(counts, patrol != ENGINE_NO_EVENT && report.ledger_index == patrol && strcmp(report.key, "patrolled") == 0,
               "report: the report names the kind of the event by its key");
    TEST_CHECK(counts, report.subject.kind == ENGINE_SUBJECT_CITY && report.subject.id == city && report.delta > 0,
               "report: the subject is the city and the amount is the rise of public order");
    TEST_CHECK(counts, report.speaker == ids[0], "report: the one officer of the command speaks");
    /* G2-018: with three officers, the last one speaks the result. */
    u16 three[SIM_DEVELOP_OFFICERS];
    feature_take_officers(api, city, three, 3);
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 50);
    TEST_CHECK(counts, api->command_train(e, city, three).refusal == SIM_REFUSAL_NONE, "report: a train with three officers runs");
    u32 trained = feature_last_event(api, SIM_EVENT_TRAINED);
    u16 speaker = trained != ENGINE_NO_EVENT ? api->event_report(e, trained).speaker : (u16)ENGINE_NO_OFFICER;
    TEST_CHECK(counts, trained != ENGINE_NO_EVENT && (speaker == three[0] || speaker == three[1] || speaker == three[2]),
               "report: one of the executors of a city command speaks (G2-018, G3-014: the original picks at random)");
    TEST_CHECK(counts, report.sign == ENGINE_SIGN_GOOD && strcmp(report.tone, "calm") == 0 && !report.report,
               "report: event_tones.tsv gives the tone calm, the rise is good news, and a patrol is no turn report");
    engine_date today = api->date(e);
    TEST_CHECK(counts, report.date.year == today.year && report.date.month == today.month && report.date.day == today.day,
               "report: an event of this turn has the date of today");

    u32 spent = feature_last_event(api, SIM_EVENT_GOLD_SPENT);
    engine_event_report cost = api->event_report(e, spent);
    TEST_CHECK(counts, spent != ENGINE_NO_EVENT && cost.sign == ENGINE_SIGN_NEUTRAL && cost.speaker == ids[0],
               "report: the gold of the command is neutral, and its officer speaks for it too");
    TEST_CHECK(counts, report.log && !cost.log,
               "report: the log shows the main effect of a command and not its cost (event_tones.tsv, column log)");

    u8 slot = feature_build(api, city, FEATURE_FACILITY_BARRACKS, setup.force);
    u32 done = feature_last_event(api, SIM_EVENT_FACILITY_COMPLETED);
    engine_event_report completed = api->event_report(e, done);
    TEST_CHECK(counts, slot != ENGINE_ANY_SLOT && done != ENGINE_NO_EVENT && completed.report && completed.sign == ENGINE_SIGN_GOOD &&
                       strcmp(completed.tone, "proud") == 0,
               "report: a finished facility is a turn report, good news, in a proud tone");
    TEST_CHECK(counts, completed.speaker != ENGINE_NO_OFFICER,
               "report: an event of the turn has a speaker: the governor of the city or the ruler");
    TEST_CHECK(counts, completed.date.days_since_start <= api->date(e).days_since_start,
               "report: the date of an event is not after today");

    engine_event_report none = api->event_report(e, api->ledger_count(e));
    TEST_CHECK(counts, none.ledger_index == ENGINE_NO_EVENT && none.speaker == ENGINE_NO_OFFICER && strcmp(none.tone, "neutral") == 0,
               "report: an index out of range gives no event");
}
