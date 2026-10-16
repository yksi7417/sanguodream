# 08 Adding a mechanic: the TDD loop end to end

Files: tests/feature/fixture_tests.cpp, spec-src/spec/{mechanics,fixtures}.
This tutorial uses "farm development" as the example because the playbook
does; the files named here do not exist until the analysis side writes them.

## The inputs
1. `spec-src/spec/mechanics/farm-development.md`: inputs, the rule in math
   notation, edge cases, confidence. Written by the analysis side from
   experiments on the original game, never from code.
2. `spec-src/spec/fixtures/farm-development.tsv`: one row per observed
   vector, header row, tab separated:
   ```
   pol  skill_farming  farm  farm_cap  gold_spent  season  farm_after  save_pair
   82   1              1200  3000      300         spring  1484        exp-017
   ```

## Step 0: the fixture fails on its own
Pull the spec (`git -C spec-src pull`), run `run_tests.bat`. The feature
layer prints:
```
FAIL 0/41 fixture/farm-development (no handler in tests/feature/fixture_tests.cpp)
```
A fixture without a handler is a failing test. That is the RED step done
for you.

## Step 1: write the handler (still RED)
In fixture_tests.cpp add a handler and register it:
```
internal void
fixture_farm_development(tsv_table *table, engine_api *api, test_counts *counts)
{
    for (u32 row = 0; row < table->row_count; ++row)
    {
        /* build the row's state through the api, run the command, compare */
    }
}
global_variable fixture_handler g_fixture_handlers[] = {
    { "farm-development", fixture_farm_development },
    { 0, 0 }
};
```
It will not compile yet, because the api has no way to create a settlement
with a farm value or an officer with politics 82. Good: that is the next
step, and it is the design step.

## Step 2: grow engine_api
Add to engine.h the queries and commands the row needs, in words the spec
uses: `create_settlement(name, farm, farm_cap)`, `create_character(pol,
skills)`, `command_develop(settlement, facility, character, gold)`,
`settlement_farm(settlement)`. Implement them in engine.cpp over new
component tables in sim_state. The handler now compiles and every row
fails on the comparison.

## Step 3: implement the rule (GREEN)
In code/sim/, a function `development_gain(...)` that is the spec's formula
and nothing else, called by the `develop` command, emitting one event per
changed quantity. Run `feature_tests.exe` until `PASS 41/41`.
If a row cannot pass and the spec looks right, the question goes to
`spec-src/spec/QUESTIONS.md`; you do not tune the formula to the data.

## Step 4: unit tests for the new pure functions
`development_gain` is a pure function: give it a unit test with the edge
cases from the spec's "Edge cases" section (winter, cap reached).

## Step 5: the UI, with its ui test
A "Develop" button in the settlement panel. The ui test: click it with the
recorder behind the panel, assert exactly one `command_develop` with the
expected arguments. No e2e test; the feature layer already proved the
engine does the right thing with that command.

## Step 6: review and commit
`/code-review` with the clean-room reminder: nothing transliterated, nothing
not traceable to a line in spec-src/spec. Commit with the spec revision
pinned (`git -C spec-src rev-parse --short HEAD` in the message).
