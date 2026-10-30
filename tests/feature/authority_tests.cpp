/* Feature tests of the authorities of the church and the two caliphs (docs/EUROPE.md, E6).
   The packs medieval_west and medieval_east have no Han emperor. Seven authorities give
   recognition, and the owner of a seat gains technique points each month. Each test
   sets up its state through engine_api. */

#define FEATURE_AUTHORITY_SLOT    24     /* slot_25.hxs; no other test uses it */
#define FEATURE_AUTHORITY_WEST    HEXDOM_DATA_ROOT "/medieval_west"
#define FEATURE_AUTHORITY_EAST    HEXDOM_DATA_ROOT "/medieval_east"
#define FEATURE_AUTHORITY_FRANCE  0      /* a Latin ruler of the west pack, not the owner of Rome */
#define FEATURE_AUTHORITY_ENGLAND 1
#define FEATURE_AUTHORITY_GHANIYA 8      /* the Banu Ghaniya of the west pack; their caliph sits in Baghdad */
#define FEATURE_AUTHORITY_PAPACY  13     /* the owner of Rome in the west pack */
#define FEATURE_AUTHORITY_ROME    11
#define FEATURE_AUTHORITY_SEGNI   55     /* Richard of Segni, an officer of the papacy who is not devout */
#define FEATURE_AUTHORITY_CAPUANO 56     /* Peter Capuano, a devout cardinal of the papacy (Q-E10) */
#define FEATURE_AUTHORITY_TECHNIQUE 50   /* authority.technique of rules.tsv */
#define FEATURE_AUTHORITY_MONTHS  12     /* interdict.months of rules.tsv */
#define FEATURE_AUTHORITY_CAIRO_SEAT 4   /* the city of Cairo in the east pack */

struct feature_authority_row
{
    const char *key;
    u8          seat_city;      /* ENGINE_NO_CITY if the seat is not on the map */
    u8          church;
};

internal void
feature_authority_start(engine_fixture *f, const char *pack)
{
    feature_world_map_init(f, pack);
    f->api.load_scenario(f->api.engine, f->api.scenario_at(f->api.engine, 0).file);
}

internal u8
feature_authority_owner(engine_api *api, u8 city_id)
{
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        if (city.city_id == city_id) return city.owner_force_id;
    }
    return ENGINE_NO_FACTION;
}

/* The row of history.tsv with this key, or ENGINE_NO_ROW. */
internal u32
feature_authority_history_row(engine_api *api, const char *key)
{
    for (u32 row = 0; row < api->history_count(api->engine); ++row)
    {
        if (strcmp(api->history_at(api->engine, row).key, key) == 0) return row;
    }
    return ENGINE_NO_ROW;
}

/* The player plays as a faction. A debate that an AI faction opened against it is closed at once. */
internal void
feature_authority_play_as(engine_api *api, u8 force)
{
    void *e = api->engine;
    feature_play_as(api, force);
    if (api->debate_view(e).active)
    {
        api->command_debate_action(e, SIM_DEBATE_AUTO, 0);
        api->command_debate_action(e, SIM_DEBATE_FINISH, 0);
    }
    if (api->pending_event(e).active) api->command_event_choice(e, 0);
}

/* The technique points that a faction has after the next month starts. It starts from zero. */
internal u32
feature_authority_month_gain(engine_api *api, u8 force)
{
    void *e = api->engine;
    api->set_technique_points(e, force, 0);
    u32 month = api->date(e).month;
    for (u32 step = 0; step < 300 && api->date(e).month == month; ++step) api->end_turn(e);
    return api->technique_points(e, force);
}

/* The rows of authorities.tsv: the engine gives each authority with its seat, its church and its keeper. */
internal void
feature_authority_rows_check(test_counts *counts, engine_api *api, const char *pack, const feature_authority_row *rows, u32 row_count)
{
    void *e = api->engine;
    char name[256];
    snprintf(name, sizeof(name), "the %s pack has %u authorities", pack, row_count);
    TEST_CHECK(counts, api->authority_count(e) == row_count, name);
    TEST_CHECK(counts, !api->authority_at(e, row_count).known, "an index after the last authority is not known");
    for (u32 index = 0; index < row_count; ++index)
    {
        engine_authority authority = api->authority_at(e, index);
        b32 right = authority.known && strcmp(authority.key, rows[index].key) == 0 &&
                    authority.seat_city == rows[index].seat_city && authority.church == rows[index].church;
        snprintf(name, sizeof(name), "the %s pack: authority %u is %s, with its seat and its church of 1200", pack, index, rows[index].key);
        TEST_CHECK(counts, right, name);
        b32 on_map = rows[index].seat_city != ENGINE_NO_CITY;
        b32 keeper_right = on_map ? authority.keeper == feature_authority_owner(api, rows[index].seat_city) : authority.keeper == ENGINE_NO_FACTION;
        snprintf(name, sizeof(name), "the %s pack: the keeper of %s is the owner of the seat, or no faction if the seat is not on the map", pack, rows[index].key);
        TEST_CHECK(counts, keeper_right, name);
        TEST_CHECK(counts, authority.holder[0] != 0, "each authority has the name of its holder for the panel");
    }
}

internal void
feature_authority_query_tests(test_counts *counts)
{
    static const feature_authority_row west[] =
    {
        { "rome", FEATURE_AUTHORITY_ROME, CONTENT_CHURCH_LATIN },
        { "constantinople", ENGINE_NO_CITY, CONTENT_CHURCH_GREEK },
        { "alexandria", ENGINE_NO_CITY, CONTENT_CHURCH_GREEK },
        { "antioch", ENGINE_NO_CITY, CONTENT_CHURCH_LATIN },
        { "jerusalem", ENGINE_NO_CITY, CONTENT_CHURCH_LATIN },
        { "abbasid", ENGINE_NO_CITY, CONTENT_CHURCH_MUSLIM },
        { "almohad", 6, CONTENT_CHURCH_MUSLIM },
    };
    static const feature_authority_row east[] =
    {
        { "rome", 11, CONTENT_CHURCH_LATIN },
        { "constantinople", 6, CONTENT_CHURCH_GREEK },
        { "alexandria", 3, CONTENT_CHURCH_GREEK },
        { "antioch", 34, CONTENT_CHURCH_LATIN },
        { "jerusalem", 40, CONTENT_CHURCH_LATIN },
        { "abbasid", 38, CONTENT_CHURCH_MUSLIM },
        { "almohad", ENGINE_NO_CITY, CONTENT_CHURCH_MUSLIM },
    };
    engine_fixture w;
    feature_authority_start(&w, FEATURE_AUTHORITY_WEST);
    feature_authority_rows_check(counts, &w.api, "west", west, ArrayCount(west));
    engine_authority rome = w.api.authority_at(w.api.engine, 0);
    TEST_CHECK(counts, rome.keeper == FEATURE_AUTHORITY_PAPACY, "in 1200 the Papal States own Rome and keep the authority of Rome");
    engine_recognition papacy = w.api.recognition(w.api.engine, FEATURE_AUTHORITY_PAPACY);
    TEST_CHECK(counts, papacy.authority == 0 && papacy.recognized && !papacy.crown && papacy.interdict_months == 0,
               "the keeper of Rome has recognition, and no crown and no interdict");
    TEST_CHECK(counts, w.api.recognition(w.api.engine, 7).authority == 6, "the Almohads have the authority of the Almohad caliph");
    TEST_CHECK(counts, w.api.recognition(w.api.engine, 7).recognized, "the owner of Marrakesh has recognition from its own authority");
    engine_recognition ghaniya = w.api.recognition(w.api.engine, FEATURE_AUTHORITY_GHANIYA);
    TEST_CHECK(counts, ghaniya.authority == 5 && !ghaniya.recognized,
               "west: Baghdad is not on the map, so the Banu Ghaniya get no recognition from the Abbasid caliph");
    TEST_CHECK(counts, w.api.recognition(w.api.engine, 250).authority == ENGINE_NO_ROW, "a faction that does not exist has no authority");
    engine_fixture_destroy(&w);

    engine_fixture h;
    feature_authority_start(&h, FEATURE_AUTHORITY_EAST);
    feature_authority_rows_check(counts, &h.api, "east", east, ArrayCount(east));
    engine_fixture_destroy(&h);

    engine_fixture plain;
    engine_fixture_init(&plain, 1, 4096);
    TEST_CHECK(counts, plain.api.authority_count(plain.api.engine) == 0 && !plain.api.authority_at(plain.api.engine, 0).known &&
                       plain.api.recognition(plain.api.engine, 0).authority == ENGINE_NO_ROW,
               "a pack of the Han emperor has no authority and no recognition");
    TEST_CHECK(counts, plain.api.command_ask_crown(plain.api.engine, 0).refusal != SIM_REFUSAL_NONE &&
                       plain.api.command_interdict(plain.api.engine, 0, 1).refusal != SIM_REFUSAL_NONE,
               "a pack of the Han emperor refuses ask_crown and interdict");
    engine_fixture_destroy(&plain);
}

internal void
feature_authority_month_tests(test_counts *counts)
{
    engine_fixture w;
    feature_authority_start(&w, FEATURE_AUTHORITY_WEST);
    u32 gain = feature_authority_month_gain(&w.api, FEATURE_AUTHORITY_PAPACY);
    TEST_CHECK(counts, gain >= FEATURE_AUTHORITY_TECHNIQUE, "west: the owner of Rome gains the technique points of the monthly rule");
    TEST_CHECK(counts, feature_authority_month_gain(&w.api, FEATURE_AUTHORITY_FRANCE) < FEATURE_AUTHORITY_TECHNIQUE,
               "west: a faction that owns no seat gains nothing from the authorities");
    u8 marrakesh = feature_authority_owner(&w.api, 6);
    TEST_CHECK(counts, feature_authority_month_gain(&w.api, marrakesh) >= FEATURE_AUTHORITY_TECHNIQUE,
               "west: the owner of Marrakesh gains the technique points for the seat of the Almohad caliph");
    /* The column devout of officers.tsv (Q-E10). The two officers get the same edit, so only the rule of the seat makes a difference. */
    engine_officer_edit edit = feature_edit_of(&w.api, FEATURE_AUTHORITY_CAPUANO);
    edit.loyalty = 80;
    w.api.set_officer(w.api.engine, FEATURE_AUTHORITY_CAPUANO, &edit);
    w.api.set_officer(w.api.engine, FEATURE_AUTHORITY_SEGNI, &edit);
    feature_authority_month_gain(&w.api, FEATURE_AUTHORITY_PAPACY);
    u32 devout = w.api.officer_detail(w.api.engine, FEATURE_AUTHORITY_CAPUANO).loyalty;
    u32 plain  = w.api.officer_detail(w.api.engine, FEATURE_AUTHORITY_SEGNI).loyalty;
    TEST_CHECK(counts, devout == plain + 1, "west: the devout cardinal of the owner of Rome gains 1 loyalty a month more than the brother of the pope");
    engine_fixture_destroy(&w);

    engine_fixture h;
    feature_authority_start(&h, FEATURE_AUTHORITY_EAST);
    static const u8 seats[] = { 11, 6, 3, 34, 40, 38 };
    for (u32 index = 0; index < ArrayCount(seats); ++index)
    {
        u8 owner = feature_authority_owner(&h.api, seats[index]);
        char name[160];
        snprintf(name, sizeof(name), "east: the owner of the seat in city %u gains the technique points of the monthly rule", seats[index]);
        TEST_CHECK(counts, feature_authority_month_gain(&h.api, owner) >= FEATURE_AUTHORITY_TECHNIQUE, name);
    }
    engine_fixture_destroy(&h);
}

internal void
feature_authority_command_tests(test_counts *counts)
{
    engine_fixture f;
    feature_authority_start(&f, FEATURE_AUTHORITY_WEST);
    engine_api *api = &f.api;
    void *e = api->engine;

    TEST_CHECK(counts, api->command_ask_crown(e, FEATURE_AUTHORITY_PAPACY).refusal != SIM_REFUSAL_NONE,
               "a faction that does not act refuses ask_crown");

    feature_authority_play_as(api, FEATURE_AUTHORITY_FRANCE);
    api->set_diplomacy(e, FEATURE_AUTHORITY_PAPACY, FEATURE_AUTHORITY_FRANCE, 10, RELATION_NONE, 0);
    api->set_diplomacy(e, FEATURE_AUTHORITY_FRANCE, FEATURE_AUTHORITY_PAPACY, 10, RELATION_NONE, 0);
    engine_recognition before = api->recognition(e, FEATURE_AUTHORITY_FRANCE);
    TEST_CHECK(counts, before.authority == 0 && !before.recognized && !before.crown,
               "France has the authority of Rome, and no recognition and no crown in 1200");
    engine_command_result cold = api->command_ask_crown(e, FEATURE_AUTHORITY_FRANCE);
    TEST_CHECK(counts, cold.refusal == SIM_REFUSAL_NOT_FRIENDLY && cold.command_id == 0,
               "ask_crown: Rome is not friendly enough, so the command is refused");

    api->set_diplomacy(e, FEATURE_AUTHORITY_PAPACY, FEATURE_AUTHORITY_FRANCE, 90, RELATION_NONE, 0);
    api->set_diplomacy(e, FEATURE_AUTHORITY_FRANCE, FEATURE_AUTHORITY_PAPACY, 90, RELATION_NONE, 0);
    u32 events_before = api->ledger_count(e);
    engine_command_result warm = api->command_ask_crown(e, FEATURE_AUTHORITY_FRANCE);
    engine_recognition after = api->recognition(e, FEATURE_AUTHORITY_FRANCE);
    TEST_CHECK(counts, warm.refusal == SIM_REFUSAL_NONE && warm.command_id != 0 && after.crown && after.recognized,
               "ask_crown: Rome is friendly, so France gets a crown and recognition");
    TEST_CHECK(counts, api->ledger_count(e) > events_before, "ask_crown writes the event crown_given to the ledger");
    TEST_CHECK(counts, api->command_ask_crown(e, FEATURE_AUTHORITY_FRANCE).refusal == SIM_REFUSAL_HAS_RECOGNITION,
               "ask_crown: a faction that has recognition cannot ask again");
    TEST_CHECK(counts, api->command_ask_crown(e, FEATURE_AUTHORITY_FRANCE).command_id == 0, "a refused ask_crown leaves the game unchanged");

    /* The owner of Rome puts a faction under interdict. */
    feature_authority_play_as(api, FEATURE_AUTHORITY_PAPACY);
    api->set_diplomacy(e, FEATURE_AUTHORITY_PAPACY, FEATURE_AUTHORITY_ENGLAND, 90, RELATION_NONE, 0);
    TEST_CHECK(counts, api->command_interdict(e, FEATURE_AUTHORITY_PAPACY, FEATURE_AUTHORITY_ENGLAND).refusal == SIM_REFUSAL_NO_TARGET,
               "interdict: a faction that is friendlier than interdict.stop is not a target");
    TEST_CHECK(counts, api->command_interdict(e, FEATURE_AUTHORITY_PAPACY, FEATURE_AUTHORITY_PAPACY).refusal == SIM_REFUSAL_NO_TARGET,
               "interdict: the owner of Rome cannot put itself under interdict");
    api->set_diplomacy(e, FEATURE_AUTHORITY_PAPACY, FEATURE_AUTHORITY_ENGLAND, 10, RELATION_NONE, 0);
    api->set_diplomacy(e, FEATURE_AUTHORITY_ENGLAND, FEATURE_AUTHORITY_PAPACY, 10, RELATION_NONE, 0);
    engine_command_result strike = api->command_interdict(e, FEATURE_AUTHORITY_PAPACY, FEATURE_AUTHORITY_ENGLAND);
    engine_recognition struck = api->recognition(e, FEATURE_AUTHORITY_ENGLAND);
    TEST_CHECK(counts, strike.refusal == SIM_REFUSAL_NONE && struck.interdict_months == FEATURE_AUTHORITY_MONTHS && !struck.recognized,
               "interdict: the target has the months of interdict.months and loses its recognition");
    TEST_CHECK(counts, api->command_interdict(e, FEATURE_AUTHORITY_PAPACY, FEATURE_AUTHORITY_ENGLAND).refusal == SIM_REFUSAL_NO_TARGET,
               "interdict: a faction under interdict is not a target again");
    TEST_CHECK(counts, api->command_interdict(e, FEATURE_AUTHORITY_FRANCE, FEATURE_AUTHORITY_ENGLAND).refusal != SIM_REFUSAL_NONE,
               "interdict: a faction that does not own Rome is refused");

    feature_next_turn(api);
    u32 months_left = api->recognition(e, FEATURE_AUTHORITY_ENGLAND).interdict_months;
    TEST_CHECK(counts, months_left <= FEATURE_AUTHORITY_MONTHS, "the months of an interdict never grow");
    engine_fixture_destroy(&f);
}

internal void
feature_authority_save_tests(test_counts *counts)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/slot_%02u.hxs", feature_save_folder, FEATURE_AUTHORITY_SLOT + 1);
    remove(path);

    engine_fixture f;
    feature_authority_start(&f, FEATURE_AUTHORITY_EAST);
    engine_api *api = &f.api;
    void *e = api->engine;
    u32 row = feature_authority_history_row(api, "caliph_to_cairo");
    TEST_CHECK(counts, row != ENGINE_NO_ROW, "the east pack has the row caliph_to_cairo");
    TEST_CHECK(counts, api->authority_at(e, 5).seat_city == 38, "in 1200 the caliph sits in Baghdad");
    TEST_CHECK(counts, !api->history_ready(e, row, 3), "the caliph stays in Baghdad while the Abbasid faction holds it");

    api->set_diplomacy(e, 6, 7, 5, RELATION_NONE, 0);
    api->set_diplomacy(e, 7, 6, 5, RELATION_NONE, 0);
    TEST_CHECK(counts, api->trigger_history(e, row, 3).refusal == SIM_REFUSAL_NONE, "the story moves the seat of the caliph to Cairo");
    engine_authority caliph = api->authority_at(e, 5);
    TEST_CHECK(counts, caliph.seat_city == FEATURE_AUTHORITY_CAIRO_SEAT && caliph.keeper == feature_authority_owner(api, FEATURE_AUTHORITY_CAIRO_SEAT),
               "the seat is Cairo, and its owner is the keeper of the caliph");

    TEST_CHECK(counts, api->command_interdict(e, 6, 7).refusal != SIM_REFUSAL_NONE,
               "the owner of Rome can command only in its own turn");
    feature_authority_play_as(api, 6);
    TEST_CHECK(counts, api->command_interdict(e, 6, 7).refusal == SIM_REFUSAL_NONE, "the owner of Rome puts Venice under interdict");
    u32 months = api->recognition(e, 7).interdict_months;
    TEST_CHECK(counts, months == FEATURE_AUTHORITY_MONTHS, "Venice has the months of the interdict");

    TEST_CHECK(counts, api->save_game(e, FEATURE_AUTHORITY_SLOT, "authority") == ENGINE_SAVE_OK, "a game with an interdict and a moved seat saves");
    api->load_scenario(e, api->scenario_at(e, 0).file);
    TEST_CHECK(counts, api->authority_at(e, 5).seat_city == 38 && api->recognition(e, 7).interdict_months == 0,
               "a new scenario puts the caliph in Baghdad and ends the interdict");
    TEST_CHECK(counts, api->load_game(e, FEATURE_AUTHORITY_SLOT) == ENGINE_SAVE_OK, "the save loads");
    TEST_CHECK(counts, api->authority_at(e, 5).seat_city == FEATURE_AUTHORITY_CAIRO_SEAT, "after the load the caliph still sits in Cairo");
    TEST_CHECK(counts, api->recognition(e, 7).interdict_months == months, "after the load Venice still has the months of the interdict");
    engine_fixture_destroy(&f);

    /* A seat that moved gives the monthly gain to its new owner. */
    engine_fixture g;
    feature_authority_start(&g, FEATURE_AUTHORITY_EAST);
    g.api.trigger_history(g.api.engine, feature_authority_history_row(&g.api, "caliph_to_cairo"), 3);
    u8 cairo_owner = feature_authority_owner(&g.api, FEATURE_AUTHORITY_CAIRO_SEAT);
    TEST_CHECK(counts, feature_authority_month_gain(&g.api, cairo_owner) >= 2 * FEATURE_AUTHORITY_TECHNIQUE,
               "after the move the owner of Cairo gains the technique points of the caliph too");
    engine_fixture_destroy(&g);
    remove(path);
    snprintf(path, sizeof(path), "%s/slot_%02u.meta", feature_save_folder, FEATURE_AUTHORITY_SLOT + 1);
    remove(path);
}

internal void
feature_authority_tests(test_counts *counts)
{
    feature_authority_query_tests(counts);
    feature_authority_month_tests(counts);
    feature_authority_command_tests(counts);
    feature_authority_save_tests(counts);
}
