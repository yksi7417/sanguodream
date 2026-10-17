# Civil War content pack

This pack is the second content pack of M9. It shows that the mechanics are
generic: the same engine code runs it, and only the data is different.

The pack starts as a copy of the placeholder pack. These items change:
- unit_types.tsv: other names, and the moves 24 (foot), 32 (cavalry) and
  16 (siege);
- weapons.tsv: other names, and 500 gold for a weapon of the smithy;
- rules.tsv: 200 action points at most, and 250 gold for a conscription;
- events.tsv: only the three generic events without Three Kingdoms
  officers (treason, the help of a spouse, the spirit of sworn brothers);
- history.tsv, ancient_officers.tsv and portraits.tsv: no rows;
- scenarios.tsv: the sample save only.

The pack is a test of the engine, not a history of the war. The map and the
officers come from the save that a player loads.

To use the pack, set engine_config.content_folder to `..\data\civil_war`.
The test feature/playable loads it (tests/feature/playable_tests.cpp).
