# 19 Court: officers, posts, personnel, diplomacy, duel and debate

Files: code/sim/officers.cpp, code/sim/diplomacy.cpp, code/sim/duel.cpp,
code/sim/debate.cpp, code/sim/content.cpp (skills.tsv, posts.tsv,
ranks.tsv, items.tsv), code/engine/engine_court.cpp, code/ui/ui_court.cpp.
The data is in data/placeholder/skills.tsv, posts.tsv, ranks.tsv,
items.tsv and rules.tsv.
Tests: tests/unit/court_tests.cpp, tests/feature/court_tests.cpp, the M7
tests in tests/ui/ui_tests.cpp, the demo "demo/m7_court".

## What it is for
This tutorial covers milestone M7: the officers as people, the court of a
ruler and the relations of factions. The specs are:
- mechanics/officers-and-relations.md: abilities, growth, health,
  loyalty, relations and items;
- mechanics/factions-corps-and-posts.md: the ranks and the posts;
- mechanics/city-commands.md: the personnel, ruler, diplomacy and plot
  commands;
- mechanics/skills.md: the 100 skills;
- mechanics/diplomacy.md, mechanics/duel.md, mechanics/debate.md and
  ui/duel.md.

The features are F-051 to F-056, F-058, F-059, F-100, F-101 and F-110 to
F-113 of FEATURES.md.

## The fields that the save does not decode
The save loader decodes the loyalty from the byte +104 of the officer
record (G2-035). The game keeps a surplus above 100 in that byte (張良
113, 李傕 242) and shows at most 100. The clone keeps the byte and shows
at most 100. The loader does not decode ambition, justice, personality,
growth type, sex or health (Q-177). After a save loads, the engine gives
each officer these assumed values:
- ambition and justice: 2, that is normal (0 to 4);
- personality: reckless for a strong officer with a low intellect, bold
  for a strong officer, calm for a wise officer, careful for the others;
- growth type: normal; health: good; stamina: 100.

The liked and disliked officers come from the save. The editor command
`set_officer` changes the assumed fields.

## The tables
- skills.tsv has one row for each skill number 1 to 100. The column
  applies says how the engine uses the skill:
  - "rule": the loader writes the number into the rule field
    skill.<effect>;
  - "table": another table names the number;
  - "none": no effect yet.
  The skill numbers left rules.tsv for this table.
- ranks.tsv has the ten ranks of a ruler: the soldiers and the cities.
- posts.tsv has the 80 posts of the guide book: merit, soldiers, the
  ability and its bonus, and the lowest rank that allows the post. The
  rank column is an assumption (Q-027).
- items.tsv finds the kind of a save item by the Big5 bytes of its name.

## Loyalty, experience and age
These rules run on the calendar:
- Each month an officer loses loyalty.monthly, more for ambition above
  normal and less for justice above normal. 仁政 and 掌握人心 halve the
  loss. The sworn brothers, spouses and liked officers of the ruler lose
  nothing.
- A city that cannot pay its salaries takes loyalty.unpaid from its
  officers.
- A loss of loyalty takes from the surplus above 100 first. A gain stops
  at 100 (the run of 2026-10-22).
- An officer below loyalty.leave can leave its faction (an officer with
  justice 4 never does).
- Each month the ruler gets the rank of its city count.
- A command gives experience.command points to the ability it uses. Each
  experience.level points add 1 to the ability, up to 100.
- With 能力變動, force and leadership fall each year after the age of
  decline of the growth type.
- With the historical lifespan, an officer older than its death year rolls
  its death at each month start (age.death_month_chance, assumed; G2-055).
  The deputy leads its unit after the death. A dead ruler gives the
  faction to the officer with the most merit.

Each turn the stamina recovers and a wound heals by one step after
health.wound_turns turns.

## Posts and the soldiers of an officer
An officer leads the soldiers of its post. A ruler leads the soldiers of
its rank, and an officer without a post leads unit.soldier_cap. 改革軍制
adds to each. 官職 needs three things:
- the merit of the post;
- a rank of the ruler that allows the post;
- a free post: one holder for each post in a faction.

The post adds its bonus to its ability.

## The personnel and ruler commands
- 探索 (20 AP): the chance is search.base plus a part of politics; 眼力
  always finds a free officer. A found officer with a high intellect
  starts a debate; the others join at the chance of a recruit. Without a
  free officer, a search can find gold.
- 登庸 (20 AP): the target is a free officer in an own or empty city, an
  own captive, or an officer of another faction. Nobody can recruit a
  ruler, an officer with loyalty 100, or a sworn brother or spouse of its
  ruler. A ruler cannot recruit an officer who dislikes it. The chance
  uses charm, loyalty, compatibility, captivity and relations. The ruler
  recruits its own sworn brothers and spouse without a roll. A target
  with a high intellect answers with a debate.
- 褒賞 (5 AP and 100 gold for each officer): one order rewards a set
  of officers of the whole force (G2-040). The city of the order pays.
  An officer in a unit, an officer away, the ruler and an officer at
  loyalty 100 get no reward. Each officer gets one reward in a turn.
  The rise is the formula `sim_reward_loyalty_rise`: reward.loyalty,
  less one for each reward.compatibility_divisor of the distance of
  compatibility to the ruler, at least reward.loyalty_min. The spec
  names the compatibility as the one factor and gives no numbers
  (Q-028). The rows have the status assumed. docs/PROGRESS-GAP2.md asks
  the observer for the data of a fit.
- The clue of the adviser (ui/city-commands.md: the estimate after the
  choice of a target): `court_advice` gives the chance of 探索 or 登庸
  and its band. The bands are advice.low_below and advice.high_from of
  rules.tsv (assumed). The words of the original are a question in
  docs/PROGRESS-GAP2.md.
- 授與 (10 AP): the item goes to the officer, and loyalty rises by its
  value.
- 仲介 (500 technique points): two officers with merit 500 become sworn
  brothers. A man and a woman with merit 500, both under 40, can marry.
  內助 adds 1 to each ability of both.
- 流放: a captive goes back to its faction and the faction gains
  technique points; an own officer becomes free and the faction loses
  technique points.

## Relations
Compatibility is a wheel of 150. The distance on the wheel lowers the
chance of a recruit and the loyalty of a reward. The relations change a
unit:
- sworn brothers and spouses give the full deputy bonus;
- a liked deputy gives more than normal;
- a disliked deputy gives nothing;
- a disliked pair lowers the unit intellect.

Relations also help a recruit and a substitute in a duel.

## Diplomacy
Each pair of factions has a friendliness of 0 to 100 (five steps of 20
points) and a relation: none, alliance or ceasefire with months. An ally
or a faction under a ceasefire is no enemy: no attack, no zone of
control, no block of commands. The commands cost 30 AP (half with a
diplomacy office):
- 親善 (1000 gold) never fails and raises friendliness;
- 同盟 (1000 gold or more), 停戰協定 (2 to 24 months), 降伏勸告, 交換俘虜 and
  請求援軍 roll a chance; a failure lowers friendliness;
- 摒棄 always works and costs technique points.

An envoy with 論客 goes to a debate for 同盟, 停戰協定 and 交換俘虜. An aid
unit of an ally stands next to the own city for aid.turns turns. A
faction that accepts a surrender gives its cities, officers and units.

The envoy goes on the road for the days of 抵達日數 to the capital of the
target (travel.cpp, G2-063). The result comes at the turn start of the
arrival, as the result of a plot does. A debate of a player starts at
the arrival. A computer faction rolls the chance.

## The plots of factions
二虎競食, 驅虎吞狼 and 流言 cost 30 AP and 200 gold. Two tigers lowers the
friendliness of two other factions. Drive the tiger makes the governor of
a city the ruler of a new faction. Rumour lowers the public order of the
city and the loyalty of its officers.

## The duel
單挑 is a unit order against an adjacent enemy unit. The chance that the
target accepts depends on its personality and force. While a duel runs,
the other commands wait (`busy`). Each round both fighters strike; the
policy changes the damage and the spirit (鬥志). To keep one policy
duel.critical_rounds rounds gives its critical. The special commands need
spirit or an item:
- 必殺技 100, 集氣 and 堅守 50, 要害 200, 無雙 300;
- 暗器 and 弓: the item.

退卻 leaves the duel; with a horse it always works. The items change the
damage and the spirit. A substitute of the same unit can take the place
of the officer. The
computer side follows its personality. The winner gains merit, technique
points and perhaps force; the loser can become a captive. At
duel.max_rounds the duel is a draw.

## The debate
Each side has a mind gauge with five rocks, an anger gauge and a hand by
intellect. The side that has the turn plays a card; the other answers
with its weakest card that beats it, or takes the hit. The order is 無視 >
大喝 > 詭辯 > topic cards > 鎮靜 > 憤怒, and a card of the current topic
beats the other topics. Anger rises with each hit; at 100 the side rages
by its personality. A side wins when the other mind gauge is empty. The
winner chooses 施予同情 (technique points) or 進逼 (perhaps 1 intellect),
and the command that started the debate gets its result.

## engine_api
- `officer_detail` (with `recruit_chance` and `reward_rise` for the
  faction that acts), `skill_kind_*`, `post_kind_*`, `rank_kind_*`,
  `faction_rank`, `post_holder`, `item_count`, `item_at`, `diplomacy`,
  `duel_view`, `debate_view` and `court_advice`;
- `command_court(kind, order)` for the commands ENGINE_COMMAND_SEARCH to
  ENGINE_COMMAND_FACTION_PLOT, `command_duel`, `command_duel_action` and
  `command_debate_action`. `can_issue` checks each of them;
- the editor commands `start_debate`, `set_officer` (with `captor` to make
  a captive), `set_item_owner`, `set_diplomacy` and `set_stamina`.

## The panels
- The Court panel shows the rank and one officer, and sends 褒賞, 流放,
  官職, 探索, 登庸, 授與, 仲介 and the release of a captive. Next to 探索
  and 登庸 it shows the clue of the adviser in the colour of the band,
  with the name of the 軍師. A hover shows the chance.
- The 褒賞 dialog (人材 > 褒賞) lists the officers of the force that can
  get a reward. Each row has a check box, 忠誠 and the rise. The dialog
  checks every row when it opens. 一併 and 一併解除 check and clear the
  rows.
  The cost line shows the gold and the action points of the checked
  set. 決定 sends one 褒賞 order with the set.
- The Diplomacy panel lists the factions with friendliness and relation,
  and sends the seven diplomacy commands and the three plots. The envoy
  is a free officer of the city that did not act. The panel shows
  抵達日數 for the chosen faction.
- The duel screen opens when a duel runs: the gauges, the policies, the
  special commands, the substitutes, a round and the automatic play.
- The debate screen opens when a debate runs: the gauges, the topic, the
  hand, the last cards and the result choice.
- The unit menu enables 單挑: a click on the enemy hex sends it.

## The tests, line by line
- court_tests.cpp (unit) checks the parsers of the four tables, the
  wheel of compatibility, the hand size and the order of the cards. It
  also checks the rise of a reward and the bands of the clue.
- court_tests.cpp (feature) checks these items through engine_api:
  - the tables, the rank and the start state of the officers;
  - 褒賞 of a set of four officers, the rise of each, 官職 and their
    refusals;
  - the estimate of the adviser for 探索 and 登庸;
  - 登庸, 授與, 仲介 and 流放;
  - 親善, 同盟, 請求援軍, 摒棄 and 交換俘虜;
  - 驅虎吞狼;
  - a duel and a debate through their actions;
  - the loyalty of one month.
- ui_tests.cpp checks that each button of the Court panel, the Diplomacy
  panel and the two screens sends one command. It also checks that the
  褒賞 dialog sends one order with the checked set. The Court panel must
  ask for the clue of the adviser.
- demo/m7_court shows the Court panel, the Diplomacy panel, a duel and a
  debate.

Run `run_tests.bat --e2e` and `run_tests.bat --demo`. Each prints
ALL PASS.
