# 09 Save loader: from a .s11 file to officers and cities

Files: code/sim/inflate.h, code/sim/save_format.h and .cpp,
code/sim/scenario_format.h and .cpp, code/sim/sim.cpp (`sim_load_save`,
`sim_load_name_index`), code/sim/turn.cpp (`sim_force_ruler`),
code/engine/engine.cpp (`engine_api_load_save`,
`engine_api_load_name_index`, `engine_api_officer_by_id`,
`engine_api_force_ruler`, `engine_api_location_name_at`),
code/ui/ui_world.cpp, misc/save_dump.cpp.
Tests:
- tests/unit/inflate_tests.cpp (35 checks)
- tests/unit/save_format_tests.cpp (64)
- tests/unit/scenario_format_tests.cpp (31)
- tests/unit/sim_tests.cpp (18 of 32)
- tests/feature/load_save_tests.cpp (43)
- tests/feature/scenario_fixture_tests.cpp (53)
- tests/feature/name_index_tests.cpp (23)
- tests/feature/display_name_tests.cpp (26)
- seven tests in tests/ui/ui_tests.cpp

## What it is for
This tutorial covers the loader half of milestone M1. The loader reads a
save file of the original game into the officer and city tables of the sim.
Every offset in it comes from a table in spec-src/spec/formats/. The code
does not mirror the binary layout as a struct. It reads one field at a time
at its offset and puts the value in a field with the name of the spec row.

## The path of one load
1. The UI calls `api->load_save(api->engine, path)`.
2. The engine reads the file through `read_entire_file` from
   `engine_config`. A relative path starts at the folder of the executable.
3. `save_load` checks the header, inflates the payload and decodes the
   records. All of this happens in the scratch arena of the engine.
4. `sim_load_save` destroys the old officer and city entities. Then it makes
   one entity for each officer and each city and emits one `save_loaded`
   event.
5. The panels read the tables through `officer_at` and `city_at`.

A file that does not load stops at step 2 or step 3. The game and the
ledger then stay as they were.

The city names come from a second file, the name index Scenario.s11. The
"Load city names" button calls `api->load_name_index(api->engine, path)`.
This command follows the same steps with `name_index_load` and
`sim_load_name_index`. The sim keeps the names apart from the officers
and the cities, so a save load does not remove them. The Cities panel
reads the name of each row through `city_name_at(city_id)`.

## Read it in this order
1. inflate.h. The save holds its game state as one zlib stream. The
   decoder reads stored, fixed and dynamic Huffman blocks and checks the
   adler32 trailer. The Huffman decoder is the canonical one: a count of
   codes for each length, and the symbols in code order. The decoder state
   comes from an arena. The caller owns the output buffer.
2. save_format.h, top to bottom. Each enum is one table of save-file.md:
   `save_container_offset`, `save_payload_offset`, `officer_record_offset`,
   `save_city_offset`, `item_record_offset`. Compare each enum with its spec
   table. The structs after them hold decoded values only.
3. save_format.cpp. `save_load` follows the loader rule of the spec. It
   reads the two lengths at 0x1B7 and inflates the bytes from 0x1BF to the
   end of the file. Then it checks the inflated length. The arena keeps the
   decoded records and gets back the payload of 1,154,785 bytes.
4. `officer_slot_is_empty`. A slot holds an officer when one of its name
   bytes is not zero. This is the rule of the slot table of the spec. The
   status byte does not decide it (see the open questions below).
5. scenario_format.h and .cpp. A scenario officer record is the save
   record cut at +152, so `scenario_load` calls `officer_decode_shared`. A
   scenario city record has its own offsets. `name_index_load` reads the
   city and the gate/port names of Scenario.s11. The province index and
   the adjacency list of a city entry are unknown (Q-017). The decoder
   reads them, but nothing uses them.
6. sim.h and sim.cpp: `officer_component`, `city_component` and
   `sim_load_save`. The components hold the fields that the game uses. The
   decoded records hold all the fields that the spec names.
7. engine.h: `engine_officer`, `engine_city`, `engine_city_name`,
   `engine_load_result` and the seven new api functions. The api structs
   are a third copy of the fields. This copy is the contract with the
   front end, so it can stay the same when the sim changes.
8. ui_world.cpp: the two tables and the two buttons.

## Names
Every name in the save and in the name index is Big5 bytes. The spec does
not name the Big5 table that maps the bytes to characters (QUESTIONS.md,
Q-013). The engine therefore keeps the bytes as they are. The panels
decode them with the text module and the Unicode BIG5.TXT table (tutorial
12). A tooltip on a name shows the bytes as hex, for example `B1E4 B77C`.
The Cities panel shows "-" in the name column until a name index loads. save_dump.exe has a `--utf8-names`
option that reads the bytes through the Windows code page 950. That option
is for people who read the output. The game never uses it.

## Names for the id columns
The save keeps a force, an officer and a location as a number. The panels
show a name in place of each number. Three queries of engine_api give the
records that the names come from:

| Query | Answer | Source in the spec |
|---|---|---|
| `officer_by_id(officer_id)` | the officer with that id, as `officer_at` gives it; `officer_id` is `ENGINE_NO_OFFICER` if no officer has the id | save-file.md, officer array |
| `force_ruler(force_id)` | the id of the officer with status 0 (君主) and that force id; `ENGINE_NO_OFFICER` if there is none | save-file.md, officer +95 and +100 |
| `location_name_at(location)` | the name bytes of a city (0..41), a gate or a port (42..86) from the name index; `location` is `ENGINE_NO_LOCATION` if no name index is loaded or the number is out of range | scenario-file.md, Scenario.s11 at 0x05A and 0x474 |

The rule of `force_ruler` is `sim_force_ruler` in code/sim/turn.cpp. The
save also has a per-force table with a ruler id. The spec does not give
its base, so the sim reads the officer table. In sample.s11, forces 0 and 12 own
cities but have no officer with status 0. Each has a leader with status
1. The panels show "force 0" and "force 12" for them (Q-149).

The force column shows the name of the ruler. The commander and governor
columns show the name of the officer. The location column shows the name
from the name index, or the number until a name index loads. Tutorial 12
gives the labels of the status and the specialty codes.

## The tests, line by line
- inflate_tests.cpp: a stored block of "hello", a fixed block with a back
  reference that overlaps its own output, and a dynamic block. Each error
  status has one test. The last group inflates the stream of sample.s11 to
  exactly 1,154,785 bytes and checks its adler32 trailer. This group is the
  one unit test that reads a file.
- save_format_tests.cpp: the tests write values at the spec offsets as
  plain numbers, not through the enums of the decoder. A wrong enum value
  then fails a test. The fill byte 0xEE in the unknown bytes makes sure
  that the decoder does not read them. `unit_save_load_tests` builds a
  whole save from stored blocks and loads it.
- scenario_format_tests.cpp: a hand-built SCEN0nn.s11 and Scenario.s11.
  Each value sits at the offset of its spec row, so a wrong enum value
  fails a test.
- sim_tests.cpp: "one officer per slot with a name", "a second load
  replaces the first", "unload destroys every officer and city entity",
  and "a name index load copies the names".
- scenario_fixture_tests.cpp: the real SCEN002.s11 and Scenario.s11
  through the loaders, because engine_api has no scenario command yet.
  The tests check the counts, the header and the officer ranges. They
  check the owner of each city that scenario-file.md names, and the
  Hanzhong values. Each city name has two bytes for each character of its
  spec name. The last group compares SCEN002 with sample.s11. It asserts
  only the fields that agree for all 850 officers (Q-018).
- name_index_tests.cpp: the real Scenario.s11 through `load_name_index`
  and `city_name_at`. A refused file changes nothing, and a save load
  keeps the names.
- load_save_tests.cpp: the real sample through the api. It checks the
  identity values that the spec gives for sample.s11. Officer 343 has stats
  96/72/91/94/96. Officer 442 rules force 9 from city 36. Officer 98 has
  sworn-brother head 635. It also checks the counts, the observed ranges
  and the cross-checks between officers and cities.
- display_name_tests.cpp: the three name queries on the real files. A new
  game has no ruler, no officer and no location name. `officer_by_id(id)`
  gives the same record as `officer_at` for each officer of sample.s11.
  Officer 442 rules force 9. Force 255 and the tribal force 42 have no
  ruler. Each ruler has status 0 and its own force id, and no officer
  rules two forces. The leader of each faction with a ruler is its ruler.
  The two factions without a ruler have a leader with status 1. After
  Scenario.s11 loads, locations 0..41 have the city names. Locations
  42..86 have the bytes of the gate and port entries. Location 87 has no
  name.
- ui_tests.cpp: each panel reads one row for each call and commands
  nothing. The Cities panel calls `city_name_at` one time for each row, with
  the id that `city_at` gave for that row. Each button sends exactly one
  command with its fixture path. The e2e test
  "load_sample_save_and_city_names_fill_the_panels" puts the real engine
  behind the two buttons.

## save_dump.exe
Run the dump on the sample:
```
build\save_dump.exe spec-src\spec\fixtures\saves\sample.s11 > sample.json
```
The JSON keys are the names in the `Meaning` column of the spec rows,
in snake case. The tool leaves unknown bytes out. It runs the cross-checks
of save-file.md (City record) and returns 1 if one fails. Compare its
output with the dump of the analysis side. A difference is a spec bug or a
loader bug. Write it in QUESTIONS.md. Do not change the loader to match
the other dump.

To add the city names, give the name index:
```
build\save_dump.exe spec-src\spec\fixtures\saves\sample.s11 --name-index spec-src\spec\fixtures\scenarios\Scenario.s11
```
Each city then has "name_big5_hex" after "id". If the name index does not
load, the tool returns 2.

## Open questions
The loader found seven gaps in the spec. Each one is in
spec-src/spec/QUESTIONS.md:
- Q-012: in sample.s11, 135 named built-in officers have status 255. The
  status row calls 255 "empty slot". The loader uses the name rule.
- Q-013: the Big5 table for names.
- Q-014 (resolved): Scenario.s11 and SCEN002.s11 are now fixtures.
- Q-015: the layout of the province table in Scenario.s11.
- Q-016: three observed ranges do not cover sample.s11. The feature tests
  do not assert these three ranges.
- Q-017: the adjacency examples of the name index are one byte after the
  offsets of the spec table. The tests do not assert the adjacency.
- Q-018: between SCEN002 and sample.s11, the aptitudes, the liked and
  disliked lists and one portrait change. The spec calls these bytes
  static. The force relations also have the values 0, 10 and 80.

Two questions come from the names of the id columns:
- Q-149: forces 0 and 12 of sample.s11 have no officer with status 0.
  The spec does not give the base of the per-force table of the save.
- Q-150: the game terms of the status values 4, 5, 7 and 8.

## Exercises
- Add `force_count` and `force_at(index)` to engine_api from the force
  records of a scenario. Write the feature test first, with a hand-built
  scenario.
- Add a "Load scenario" button. Decide what the Cities panel shows for a
  scenario city, which has no commander.
- After the analysis side answers Q-017, correct the offsets of the
  province index and the adjacency list. Then assert the two adjacency
  examples of scenario-file.md in scenario_fixture_tests.cpp.

The result you can check: `run_tests.bat --e2e` prints ALL PASS, and
save_dump.exe returns 0 for sample.s11, with and without `--name-index`.
