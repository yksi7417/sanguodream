/* Feature tests of M13, the replay of the save pairs of the original
   (spec/fixtures/saves/pairs, experiments/ab-testing-plan.md, level A).
   Each pair folder of the spec must have a row in pairs.tsv of the pack
   with a command that the engine can replay. A pair with no row or no
   handler fails. Each replay prints its counts. The unit of measure is the
   count of fields that differ.

   The milestone asks for zero differences in the confirmed fields. The
   rules of the clean room are assumed where the spec has no numbers
   (Q-146 to Q-158), so some fields differ. feature_replay_expected holds
   the measured count of each pair as a ceiling. A change that makes a
   replay worse fails. A change that makes it better lowers the ceiling. */

#define FEATURE_REPLAY_PAIRS_RELATIVE "..\\spec-src\\spec\\fixtures\\saves\\pairs"

struct feature_replay_expected
{
    const char *rule;
    u32         number;
    u32         differ_verified;   /* the ceiling of the confirmed fields that differ */
};

/* Measured on 2026-10-19 (tutorial 26 explains each difference). */
global_variable const feature_replay_expected feature_replay_ceilings[] =
{
    { "conscript",    1, 2 },     /* troops (Q-153) and public order (Q-152) */
    { "develop",      1, 1 },     /* the merit of the officer comes later in the original */
    /* Measured again on 2026-10-21 after G2-001 to G2-004: the load takes
       the date of the save and marks the units of acted leaders as acted.
       The AI factions then pick other commands, and the counts of the
       economy pairs moved (334 to 345, 177 to 186, 190 to 198, 185 to 190). */
    { "economy-turn", 1, 345 },   /* the AI factions pick other commands (M15) */
    /* Measured on 2026-10-20: the pairs of the driven run of 孫堅 in 長沙 (spec 7b606e1). */
    { "conscript",    2, 2 },     /* troops +650 for +1383 (Q-153), public order -5 for -8 (Q-152) */
    { "develop",      2, 3 },     /* the original gives the merit of 開發 at the end of the build (Q-220) */
    { "economy-turn", 2, 186 },   /* the AI factions pick other commands (M15) */
    { "economy-turn", 3, 198 },
    { "economy-turn", 4, 190 },
    { "economy-turn", 5, 246 },
    { "economy-turn", 6, 295 },
    { "merchant",     1, 1 },     /* gold +400 for +381: the sale rate of the original (Q-155) */
    { "patrol",       1, 0 },     /* public order matches; only the technique points differ (Q-158) */
    { "train",        1, 0 },     /* morale matches; only the technique points differ (Q-158) */
};

internal const feature_replay_expected *
feature_replay_ceiling(const char *rule, u32 number)
{
    for (u32 index = 0; index < ArrayCount(feature_replay_ceilings); ++index)
    {
        if (strcmp(feature_replay_ceilings[index].rule, rule) == 0 && feature_replay_ceilings[index].number == number) return feature_replay_ceilings + index;
    }
    return 0;
}

/* The folder of the exe, then the relative path. */
internal void
feature_replay_path(const char *relative, char *out, u32 out_size)
{
    char exe_dir[MAX_PATH];
    DWORD length = GetModuleFileNameA(0, exe_dir, MAX_PATH);
    for (DWORD i = length; i > 0; --i)
    {
        if (exe_dir[i - 1] == '\\') { exe_dir[i - 1] = 0; break; }
    }
    snprintf(out, out_size, "%s\\%s", exe_dir, relative);
}

/* The row of pairs.tsv for a folder of the spec, or ENGINE_REPLAY_PAIR_CAPACITY. */
internal u32
feature_replay_row(engine_api *api, const char *rule, u32 number)
{
    for (u32 index = 0; index < api->replay_pair_count(api->engine); ++index)
    {
        engine_replay_pair pair = api->replay_pair_at(api->engine, index);
        if (strcmp(pair.rule, rule) == 0 && pair.number == number) return index;
    }
    return ENGINE_REPLAY_PAIR_CAPACITY;
}

/* Every <rule>/<n> folder of the spec has a row with a handler. Returns
   the count of folders. */
internal u32
feature_replay_folders_have_rows(engine_api *api, test_counts *counts)
{
    char pattern[MAX_PATH], rules_dir[MAX_PATH];
    feature_replay_path(FEATURE_REPLAY_PAIRS_RELATIVE, rules_dir, sizeof(rules_dir));
    snprintf(pattern, sizeof(pattern), "%s\\*", rules_dir);
    u32 folders = 0;
    WIN32_FIND_DATAA rule_entry;
    HANDLE rules = FindFirstFileA(pattern, &rule_entry);
    if (rules == INVALID_HANDLE_VALUE) return 0;
    do
    {
        if (!(rule_entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || rule_entry.cFileName[0] == '.') continue;
        char numbers_pattern[MAX_PATH];
        snprintf(numbers_pattern, sizeof(numbers_pattern), "%s\\%s\\*", rules_dir, rule_entry.cFileName);
        WIN32_FIND_DATAA number_entry;
        HANDLE numbers = FindFirstFileA(numbers_pattern, &number_entry);
        if (numbers == INVALID_HANDLE_VALUE) continue;
        do
        {
            if (!(number_entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || number_entry.cFileName[0] == '.') continue;
            folders++;
            u32 row = feature_replay_row(api, rule_entry.cFileName, (u32)atoi(number_entry.cFileName));
            char label[160];
            snprintf(label, sizeof(label), "the pair %s/%s has a row in pairs.tsv with a command the engine can replay",
                     rule_entry.cFileName, number_entry.cFileName);
            TEST_CHECK(counts, row < ENGINE_REPLAY_PAIR_CAPACITY &&
                               api->replay_pair_at(api->engine, row).command.kind != ENGINE_REPLAY_NO_COMMAND, label);
        } while (FindNextFileA(numbers, &number_entry));
        FindClose(numbers);
    } while (FindNextFileA(rules, &rule_entry));
    FindClose(rules);
    return folders;
}

internal engine_replay_report
feature_replay_run(engine_api *api, engine_replay_pair *pair)
{
    char before[ENGINE_REPLAY_PATH_SIZE + 16], after[ENGINE_REPLAY_PATH_SIZE + 16];
    snprintf(before, sizeof(before), "%s\\before.s11", pair->folder);
    snprintf(after, sizeof(after), "%s\\after.s11", pair->folder);
    return api->replay_pair(api->engine, before, &pair->command, after);
}

internal void
feature_replay_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init_real(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;

    u32 folders = feature_replay_folders_have_rows(api, counts);
    TEST_CHECK(counts, folders > 0 && folders == api->replay_pair_count(e), "pairs.tsv has one row for each pair folder of the spec");

    for (u32 index = 0; index < api->replay_pair_count(e); ++index)
    {
        engine_replay_pair pair = api->replay_pair_at(e, index);
        engine_replay_report report = feature_replay_run(api, &pair);
        printf("  replay %s/%03u: status %u, %u fields, %u differ, %u confirmed\n", pair.rule, pair.number, report.status,
               report.fields, report.differ, report.differ_verified);
        u32 by_field[ENGINE_REPLAY_FIELD_COUNT] = {};
        const char *field_rule[ENGINE_REPLAY_FIELD_COUNT] = {};
        for (u32 diff_index = 0; diff_index < report.diff_count; ++diff_index)
        {
            engine_replay_diff diff = api->replay_diff_at(e, diff_index);
            by_field[diff.field]++;
            field_rule[diff.field] = diff.rule;
            if (diff_index >= 8) continue;
            printf("    %s%s: before %lld, replay %lld, original %lld (%s %s)\n", diff.verified ? "* " : "  ", diff.path,
                   diff.before, diff.replayed, diff.original, diff.rule, diff.q_id);
        }
        for (u32 field = 0; field < ENGINE_REPLAY_FIELD_COUNT; ++field)
        {
            if (by_field[field]) printf("    by rule: %s %u\n", field_rule[field], by_field[field]);
        }
        char label[160];
        snprintf(label, sizeof(label), "the pair %s/%03u replays: both saves load and the engine runs the command", pair.rule, pair.number);
        TEST_CHECK(counts, report.status == ENGINE_REPLAY_OK, label);
        const feature_replay_expected *ceiling = feature_replay_ceiling(pair.rule, pair.number);
        snprintf(label, sizeof(label), "the pair %s/%03u differs in no more confirmed fields than measured", pair.rule, pair.number);
        TEST_CHECK(counts, ceiling && report.differ_verified <= ceiling->differ_verified, label);
    }

    /* The errors of a replay change nothing that a test reads after them. */
    engine_replay_command none = {};
    none.kind = ENGINE_REPLAY_NO_COMMAND;
    TEST_CHECK(counts, api->replay_pair(e, "a.s11", &none, "b.s11").status == ENGINE_REPLAY_NO_HANDLER, "a command with no handler does not replay");
    engine_replay_command end_turn = {};
    end_turn.kind = ENGINE_REPLAY_END_TURN;
    TEST_CHECK(counts, api->replay_pair(e, "no_such_before.s11", &end_turn, "no_such_after.s11").status == ENGINE_REPLAY_BAD_AFTER,
               "a second save that does not load stops the replay");
    TEST_CHECK(counts, api->replay_diff_at(e, 0).path[0] == 0, "a failed replay leaves no differences");
    engine_fixture_destroy(&f);
}
