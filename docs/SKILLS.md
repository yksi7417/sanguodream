# Skills and traits

This document says how a skill gets its behaviour, and how a game pack gives a behaviour to any officer.

## The three layers

| Layer | Where | What it is |
|---|---|---|
| Trait | code/sim/content.h, `SKILL_TRAITS` | One behaviour of the code, with a key. The code asks for a trait and never for a skill. |
| Skill | skills.tsv of the pack | A row with a key and a list of traits. |
| Officer | officers.tsv of the scenario, column `skill` | The key of one skill. Any officer can have any skill. |

A skill applies to the whole unit. The holder can be the commander or a deputy (spec-src/spec/mechanics/skills.md).

## The way to change a behaviour

- To give 火神 to another officer, write `fire_god` in the column `skill` of that officer.
- To make a skill of a pack, add a row to skills.tsv after the row 100. Give it a new number, a new `file_id` and a list of traits. The limit is 200 rows.
- To change a skill, change its list of traits. For example, `fire_immune|fire_power` makes a 火神 that can fail.
- To name a skill in the text of the pack, add `skill.<key>` to text/<locale>.tsv.
- To add a new behaviour, add a trait to `SKILL_TRAITS` and make the sim ask for it. The list holds 64 traits at most.

The loader refuses a trait key that the code does not know. It names the bad row in the status of the pack.

## The tables that name a skill

Some behaviours have a condition or a target. A table holds these behaviours, so a pack can add rows for any skill number.

| Table | The behaviour |
|---|---|
| critical_skills.tsv | An attack or a tactic is always critical, by class and condition. |
| plot_skills.tsv | A plot always succeeds, is critical, or has two times the chance, by plot and condition. |
| plots.tsv, `needs_skill` | The skill lets the unit use the plot. |
| terrain.tsv, `loss_waiver_skill` | The skill stops the loss on the terrain. |
| disasters.tsv, `skill_block`, `skill_chance` | The skill stops a disaster or changes its chance. |
| bandit_regions.tsv | The cities where a skill with `bandit_friend` works. This table is optional. Without it, the skill works in each city. |

A feature test checks that each skill of the pack has a trait or a row in a table.

## The traits

| Trait | The behaviour | Skills of the pack |
|---|---|---|
| `zoc_land` | The unit ignores the zone of control on land. | 飛將, 遁走 |
| `zoc_water` | The unit ignores the zone of control on water. | 推進 |
| `move_land` | More movement for a land class. | 強行 |
| `move_cavalry` | More movement for cavalry. | 長驅 |
| `move_water` | More movement on water. | 操舵 |
| `move_transport` | More movement for a transport unit. | 運搬 |
| `rockfall_guard` | Half the damage of a rockfall. | 踏破 |
| `morale_hit_small` | The attacked unit loses more morale. | 掃討 |
| `morale_hit_large` | The attacked unit loses much more morale. | 威風 |
| `morale_on_destroy` | The morale rises when the unit destroys a unit. | 昂揚 |
| `double_attack` | The unit can attack two times. | 連戰 |
| `no_counter_land` | On land, the unit can avoid the counterattack. | 急襲 |
| `no_counter_water` | On water, the unit can avoid the counterattack. | 強襲 |
| `joint_confuse` | A joint attack can confuse the target. | 掎角 |
| `capture_all` | A destroyed unit always gives its officers as captives. | 捕縛 |
| `double_technique` | Two times the technique points for a destroyed unit. | 精妙 |
| `plunder_items` | A destroyed unit gives the items of its officers. | 強奪 |
| `heart_attack` | A part of the losses of the target join the unit. | 心攻 |
| `siege_range` | A catapult or a tower shoots 1 hex farther. | 射程 |
| `mounted_archery` | Cavalry can shoot without the technique. | 白馬 |
| `aide` | Own units next to the target strike after an attack. | 輔佐 |
| `cavalry_confuse` | A cavalry tactic confuses a target with a lower force. | 疾走 |
| `forest_arrows` | Arrows work into a forest. | 射手 |
| `push_injures` | A tactic that moves a unit can injure its commander. | 猛者 |
| `last_stand` | With few soldiers, an attack can do nothing. | 不屈 |
| `weak_guard` | A weak attack can do nothing. | 金剛 |
| `joint_guard` | A joint attack counts as a normal attack. | 鐵壁 |
| `hit_morale` | The morale rises when an enemy tactic hits the unit. | 怒髮 |
| `rattan` | Half the damage of an attack, and two times the damage of fire. | 藤甲 |
| `fortune` | The officer is never captured or injured. | 強運 |
| `no_capture` | The officer of a destroyed unit is not captured. | 血路 |
| `guard` | The officers of the unit are never injured. | 護衛 |
| `see_through_lower` | The unit stops the plots from a lower intellect. | 看破 |
| `see_through_all` | The unit stops all plots. | 洞察 |
| `plot_thrift` | A plot costs 1 less morale. | 百出 |
| `plot_range` | A plot reaches 1 hex farther. | 鬼謀 |
| `plot_chain` | A plot also hits the enemy units next to the target. | 連環 |
| `plot_return` | A plot that hits the unit goes back to the unit that used it. | 反計 |
| `fire_immune` | No fire starts on the unit. Fire does no damage to the unit, and a fire plot on the unit fails. | 火神 |
| `fire_power` | The fire and the fire tactics of the unit do two times the damage. | 火神 |
| `fire_sure` | The fire plots and the fire tactics of the unit always succeed. | 火神 |
| `no_false_report` | The unit is immune to 偽報. | 規律, 明鏡 |
| `no_confusion` | The unit is never confused. | 沈著, 明鏡 |
| `music` | The morale rises each turn. | 樂奏 |
| `music_double` | Two times the morale of a 軍樂台. | 詩想 |
| `fast_build` | Two times the work on a military facility. | 築城 |
| `base_no_grain` | Next to an own port or pass, the soldiers use no grain. | 屯田 |
| `more_soldiers` | A conscription gives more soldiers. | 名聲 |
| `more_weapons` | More pikes, halberds and crossbows. | 能吏 |
| `more_horses` | More war horses. | 繁殖 |
| `fast_siege` | Siege weapons take half the time. | 發明 |
| `fast_ships` | Ships take half the time. | 造船 |
| `cheap_research` | Research costs half the gold. | 指導 |
| `sure_search` | A search always finds a free officer. | 眼力 |
| `debate_diplomacy` | Diplomacy goes to a debate. | 論客 |
| `gold_income` | 1.5 times the gold of the city. | 富豪 |
| `grain_income` | 1.5 times the grain of the city. | 米道 |
| `gold_every_turn` | Gold comes each turn, each payment halved. | 徵稅 |
| `grain_every_month` | Grain comes each month, each payment halved. | 徵收 |
| `bandit_friend` | No bandits in the city. | 親烏, 親羌, 親越, 親蠻 |
| `loyal_city` | The loyalty in the city drops less. | 仁政 |
| `marriage_bonus` | A marriage gives +1 to all abilities. | 內助 |

## Fire and confusion

Fire and confusion are independent. The player decided this on 2026-11-01.

- A hex burns, and a unit has a state. One does not change the other.
- The map shows flames for a fire and a spiral only for 混亂.
- These causes can confuse a unit: 擾亂, 妖術, a confusion tactic, 掎角, 疾走, a 石兵八陣, and a large 業火種.
- A large 業火種 rolls fire.seed_confuse_chance for each unit that it hits.

## The decisions of the player that differ from the spec

- skills.md says that 火計 of 火神 always succeeds against a lower intellect. The player wants it to succeed always. The trait `fire_sure` does this.
- 洞察 (`see_through_all`) still stops the fire plot of 火神. A unit with `fire_immune` also stops it. The two skills name the same rule from the other side.

## The assumed amounts

Q-234 in spec-src/spec/QUESTIONS.md lists the amounts that have no source. The rules.tsv of each pack holds them, so a pack can change them.

The support attack without 輔佐 does not exist yet. The spec names it only in a knowledge file, with a relation between the officers.
