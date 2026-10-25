# Community wish list: user stories

This document records the wishes of players for a new 三國志 as user
stories for hexdom.

## Source and purpose

The source is a YouTube video by the channel 三國瑜雲. The video is 19:11
long. In the video, the host reads the wishes of active players and comments
on each wish. This document uses 15 screenshots of the video, with the file
names 3 to 17. Each screenshot shows one note card. The slide number in this
document is the number of the file name.

The stories say what the players want. They do not say how to build it.
This document has no design, no plan and no estimate. Each story changes
RTK XI, so the scope of each story is "Beyond the original". The spec in
`spec-src/spec/` describes the original game. Thus these stories stay out of
the spec.

## Summary

| ID | Title | Theme | Slide | Stars | Underlined |
|---|---|---|---|---|---|
| US-001 | Fog of war | Information and fog of war | 3, 4 | 1, 2 | No |
| US-002 | Spies that look into a target city | Information and fog of war | 3 | 1 | No |
| US-003 | More routes between bases | The map and the regions | 5 | 1 | No |
| US-004 | Correct position of Yangping Pass | The map and the regions | 5 | 1 | No |
| US-005 | The region of Jiaozhou | The map and the regions | 5 | 1 | Yes |
| US-006 | The island of Yizhou | The map and the regions | 5 | 1 | Yes |
| US-007 | Dig a river | The map and the regions | 5 | 1 | No |
| US-008 | More cities | The map and the regions | 6 | 2 | No |
| US-009 | More ports and more places to land | The map and the regions | 6 | 2 | No |
| US-010 | Better roads | The map and the regions | 4 | 2 | No |
| US-011 | Troops in forts and in buildings outside a city | Bases and facilities | 4, 6 | 2, 2 | Yes |
| US-012 | Facilities with a clear use | Bases and facilities | 4 | 2 | No |
| US-013 | Remove facilities and roads with no use | Bases and facilities | 4 | 2 | No |
| US-014 | More slots for domestic buildings | Bases and facilities | 6 | 2 | No |
| US-015 | Free position of the domestic slots | Bases and facilities | 6 | 2 | No |
| US-016 | Balance of fire traps | Bases and facilities | 3 | 1 | Yes |
| US-017 | Two or three skills for each officer | Officers and skills | 6 | 2 | Yes |
| US-018 | Balance of the strength of skills | Officers and skills | 7 | 2 | No |
| US-019 | Defence skills with an effect | Officers and skills | 7 | 2 | Yes |
| US-020 | Stats of the late-period officers | Officers and skills | 15 | 3 | No |
| US-021 | Decline of old officers | Officers and skills | 15 | 3 | No |
| US-022 | Faster growth of young officers | Officers and skills | 15 | 3 | Yes |
| US-023 | Search for officers among the people | People and life | 8 | 2 | No |
| US-024 | Officers have children | People and life | 8 | 2 | Yes |
| US-025 | Raise and train children | People and life | 8 | 2 | No |
| US-026 | Behead a captive | People and life | 9 | 2 | No |
| US-027 | Kill an envoy | People and life | 9 | 2 | Yes |
| US-028 | Equipment stays with a dead officer | People and life | 10 | 3 | No |
| US-029 | Dig up a tomb | People and life | 10 | 3 | Yes |
| US-030 | Burial of a dead officer | People and life | 10 | 3 | No |
| US-031 | Population limit | Economy and population | 11 | 3 | Yes |
| US-032 | Conscription limit | Economy and population | 11 | 3 | Yes |
| US-033 | Weather over the whole land | Weather and seasons | 13 | 3 | Yes |
| US-034 | Hard marches in winter | Weather and seasons | 13 | 3 | Yes |
| US-035 | Classes of ships | The navy | 14 | none shown | No |
| US-036 | Large ships for money | The navy | 14 | none shown | Yes |
| US-037 | Two or three battle systems | Battle systems | 17 | 5 | No |
| US-038 | Save an online game | Multiplayer | 16 | 4 | No |
| US-039 | Rules of an online game | Multiplayer | 16 | 4 | Yes |
| US-040 | Messages inside the game | Multiplayer | 16 | 4 | No |
| US-041 | Rank list for duels | Multiplayer | 16 | 4 | No |
| US-042 | Online game with paid items (joke) | Multiplayer | 12 | 3 | No |

## How to read a story

- The Source row gives the slide and the line on the note card.
- The Original text row quotes the note card. A slash separates two lines.
- The Host comment row quotes the subtitle under the card.
- The Stars row gives the count of gold stars over the head of the cartoon.
  Read the note on the stars before you use this count.
- The Underlined row tells if a red line is under the wish on the card. The
  red line marks the point that the host stresses.
- The acceptance criteria are observable statements in the form Given, When,
  Then. They say what a player sees, not how the game does it.
- An open question is a point that the source does not make clear.

## Notes on the source

- The star count can be the score that the host gives each wish. But the
  count also rises through the video. Slide 5 at 4:46 has 1 star, and slide
  17 at 16:01 has 5 stars. Thus the stars can show the position in the
  video, not a score.
- Slide 14 has no row of stars. A portrait of an officer replaces the
  cartoon.
- The cards of slides 4, 7 and 15 use simplified characters on most lines.
  This document quotes each card as it is.
- On slide 3, "310" is short for 三國志10. On slide 8, "312" and "313" are
  short for 三國志12 and 三國志13.
- Some lines end with "XD", a sign of a joke. These lines are on slides 3,
  12 and 16.
- The card of slide 16 has the line 可以賺多一點. Story US-040 records it.

## 1. Information and fog of war

### US-001 Fog of war

| Field | Value |
|---|---|
| Source | Slide 3, line 1. Slide 4, line 1 (second item). |
| Original text | 如果有戰爭迷霧 (slide 3) / 战争迷雾 (slide 4) |
| Host comment | none |
| Stars | 1 (slide 3), 2 (slide 4) |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want the map to hide enemy units and
bases out of my sight, so that I must scout first.

**Acceptance criteria**

- Given an enemy unit out of my sight, when I look at the map, then the map
  does not show that unit.
- Given an enemy unit that moves into the sight of my unit, when the move
  ends, then the map shows that unit.
- Given an enemy base out of my sight, when I open its panel, then the panel
  does not show its current troops.

**Open question.** Does the fog also hide the terrain? Does the map show the
last known state of a base that the player cannot see now?

### US-002 Spies that look into a target city

| Field | Value |
|---|---|
| Source | Slide 3, lines 2, 4, 5, 6 and 7 |
| Original text | 可以考慮弄像全戰的間諜 / 偷看用的XD / 印象中310好像有類似的系統？ / 三國志7有間諜 / 好似可以看目標城有多少兵 |
| Host comment | none |
| Stars | 1 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want to send a spy into an enemy city, so
that I can see how many troops it holds.

**Acceptance criteria**

- Given a spy of my force in an enemy city, when I open the panel of that
  city, then the panel shows its troops.
- Given fog of war and no spy in an enemy city, when I open its panel,
  then the panel does not show its troops.
- Given a spy order, when I choose the target, then I can choose any enemy
  city.

**Open question.** The card names the spies of Total War, of 三國志7 and
possibly of 三國志10. What model do the players want? Is a spy an
officer or a separate agent? Can the enemy catch a spy?

## 2. The map and the regions

### US-003 More routes between bases

| Field | Value |
|---|---|
| Source | Slide 5, line 1. Time 4:46. |
| Original text | 路線可以多條一點 |
| Host comment | none |
| Stars | 1 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want more routes between bases, so that I
can choose from several paths for an attack.

**Acceptance criteria**

- Given the new map, when I count the routes between neighbour bases, then
  more pairs have two routes or more than in RTK XI.

**Open question.** What pairs of bases need a new route? Is a route a road,
a mountain path or a river path?

### US-004 Correct position of Yangping Pass

| Field | Value |
|---|---|
| Source | Slide 5, line 2 |
| Original text | 陽平關位置修正一下 |
| Host comment | none |
| Stars | 1 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a history fan, I want 陽平關 at its historical position, so
that the map agrees with the geography of the period.

**Acceptance criteria**

- Given the map, when I look at 陽平關, then the pass is at its historical
  position relative to 漢中 and the routes into 蜀.

**Open question.** The card does not say what is wrong with the position.
What source gives the correct position?

### US-005 The region of Jiaozhou

| Field | Value |
|---|---|
| Source | Slide 5, line 3 |
| Original text | 希望可以加上交州和夷洲 |
| Host comment | none |
| Stars | 1 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a history fan, I want the region of 交州 on the map, so that
the map includes the far south.

**Acceptance criteria**

- Given a new game, when I look at the map, then the map shows 交州 with one
  base or more.
- Given a base in 交州, when a force takes it, then the force can rule it as
  any other base.

**Open question.** What bases and what officers does 交州 have? Does it
have its own officers, for example the family of 士燮?

### US-006 The island of Yizhou

| Field | Value |
|---|---|
| Source | Slide 5, line 3 |
| Original text | 希望可以加上交州和夷洲 |
| Host comment | 夷州必須要 |
| Stars | 1 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a history fan, I want 夷洲 on the map, so that I can send a
fleet there as 孫權 did in 230.

**Acceptance criteria**

- Given a new game, when I look at the map, then the map shows 夷洲 as an
  island east of the coast.
- Given a fleet at a port on the coast, when I order a move to 夷洲, then
  the fleet can reach the island.

**Open question.** Does 夷洲 have a base, a population or officers? What
does a force get from the island?

### US-007 Dig a river

| Field | Value |
|---|---|
| Source | Slide 5, line 4 |
| Original text | 那來挖個河好了 |
| Host comment | none |
| Stars | 1 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want to dig a new river, so that I can
make a new water route.

**Acceptance criteria**

- Given a dig order between two areas of water, when the work ends, then
  ships can use the new water route.

**Open question.** This line can be a joke on the line before it. Is it a
real wish? Where can a player dig?

### US-008 More cities

| Field | Value |
|---|---|
| Source | Slide 6, line 5 |
| Original text | 增加城池數目 |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want more cities on the map, so that the
land has more targets and more fronts.

**Acceptance criteria**

- Given a new game, when I count the cities, then the count is larger than
  in RTK XI.

**Open question.** What new cities do the players want? Does the wish include small bases, or
only cities?

### US-009 More ports and more places to land

| Field | Value |
|---|---|
| Source | Slide 6, line 5 |
| Original text | 增加港口/登陸位置 |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want more ports and more places to land,
so that I can attack from more points on the water.

**Acceptance criteria**

- Given a new game, when I count the ports, then the count is larger than in
  RTK XI.
- Given a fleet at a new place to land, when I order the troops ashore, then
  the troops land there.

**Open question.** What coasts and what river banks get the new ports and
the new places to land?

### US-010 Better roads

| Field | Value |
|---|---|
| Source | Slide 4, line 3 |
| Original text | 还有道路优化 |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want a better network of roads, so that my
armies move between bases on paths that make sense.

**Acceptance criteria**

- Given the map, when I look at a road, then the road leads to a base, a
  pass, a port or another road.

**Open question.** The card does not say what is wrong with the roads. Are
there too many roads, too few, or roads at the wrong place?

## 3. Bases and facilities

### US-011 Troops in forts and in buildings outside a city

| Field | Value |
|---|---|
| Source | Slide 4, line 1 (first item). Slide 6, line 4. |
| Original text | 城塞驻兵 (slide 4) / 城外駐軍建築 (slide 6) |
| Host comment | 像我就非常喜歡三國志9的城塞駐兵 (slide 4) |
| Stars | 2 (slide 4), 2 (slide 6) |
| Underlined | Yes (slide 4) |
| Scope | Beyond the original |

**Story.** As a strategy player, I want to station troops in forts outside a
city, as in 三國志9, so that I can defend a border.

**Acceptance criteria**

- Given a fort of my force, when I move troops into it, then the fort keeps
  the troops until I move them out.
- Given a fort with troops, when an enemy unit attacks it, then the troops in
  the fort defend it.
- Given a city of my force, when I order a building for troops outside the
  city, then the building can hold troops.

**Open question.** Do the troops in a fort count as troops of the city? Can
an officer command from a fort?

### US-012 Facilities with a clear use

| Field | Value |
|---|---|
| Source | Slide 4, line 1 (third item) and line 2 |
| Original text | 设施优化 （石兵八阵这么好的创意可惜了） |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want facilities such as the 石兵八陣 to
have a clear use, so that each facility is worth its cost.

**Acceptance criteria**

- Given a 石兵八陣 on the map, when an enemy unit comes near it, then the
  facility has an effect that changes the battle.
- Given any facility, when I read its description, then the description
  gives its effect, and the effect occurs in play.

**Open question.** The card does not say what is wrong with the facilities.
Are they too weak, too costly or too late in the technology?

### US-013 Remove facilities and roads with no use

| Field | Value |
|---|---|
| Source | Slide 4, line 4 |
| Original text | 不必要的就撤了吧 |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a player, I want the game to drop facilities and roads that
have no use, so that the map stays clear.

**Acceptance criteria**

- Given the list of facilities to build, when I open it, then each facility
  has a use that no other facility gives.

**Open question.** Does the line refer to the facilities, to the roads, or
to both?

### US-014 More slots for domestic buildings

| Field | Value |
|---|---|
| Source | Slide 6, line 2 |
| Original text | 各城內政格增加/範圍內 |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a player, I want more slots for domestic buildings in each
city, so that I can develop each city further.

**Acceptance criteria**

- Given a city, when I count its slots for domestic buildings, then the
  count is larger than in RTK XI.

**Open question.** The text 範圍內 means "inside the range". Must the new
slots stay inside the territory of the city?

### US-015 Free position of the domestic slots

| Field | Value |
|---|---|
| Source | Slide 6, line 3 |
| Original text | 自由安排內政格位置 |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a player, I want to choose where the slots for domestic
buildings go, so that I can plan the layout of my city.

**Acceptance criteria**

- Given a hex in the territory of my city, when I mark it as a slot, then I
  can put a domestic building on it.

**Open question.** Can a slot go on any terrain? Is there a limit on the
count of slots?

### US-016 Balance of fire traps

| Field | Value |
|---|---|
| Source | Slide 3, line 3 |
| Original text | 还有火陷阱的平衡 |
| Host comment | 還有火陷阱平衡 |
| Stars | 1 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a strategy player, I want fire traps with a fair strength, so
that they are a fair choice in battle.

**Acceptance criteria**

- Given a fire trap, when I read its description, then the description gives
  its damage and the conditions of its effect.
- Given a fire trap and a different facility of the same cost, when I compare
  their effects, then neither one is clearly the better choice.

**Open question.** Are fire traps too strong or too weak? The card does not
say. The card puts this line near the fog of war. Thus the problem can be
traps that a player cannot see.

## 4. Officers and skills

### US-017 Two or three skills for each officer

| Field | Value |
|---|---|
| Source | Slide 6, line 1 |
| Original text | 雙技能/三技能 |
| Host comment | 其實根本就不應該只有一個特技 |
| Stars | 2 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a player, I want an officer to have two or three skills, so
that each officer has more ways to be useful.

**Acceptance criteria**

- Given an officer, when I open the panel of the officer, then the panel
  shows up to three skills.
- Given an officer with two skills, when the conditions of each skill occur,
  then each skill has its effect.

**Open question.** Do all officers have more than one skill, or only some?
Can a player teach a skill to an officer?

### US-018 Balance of the strength of skills

| Field | Value |
|---|---|
| Source | Slide 7, line 1 |
| Original text | 平衡一下特技的强度 |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want skills with a balanced strength, so
that no single skill decides most battles.

**Acceptance criteria**

- Given the list of skills, when a player compares them, then no skill is
  much stronger than all others.

**Open question.** What skills are too strong now? The card gives no names.

### US-019 Defence skills with an effect

| Field | Value |
|---|---|
| Source | Slide 7, line 2 |
| Original text | 让防御特技有点作用 |
| Host comment | 然後要開一個新遊戲 |
| Stars | 2 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a strategy player, I want defence skills with a clear
effect, so that an officer with one is a good choice.

**Acceptance criteria**

- Given two equal units, one with a defence skill, when the same enemy
  attacks each, then the unit with the skill loses fewer troops.

**Open question.** What skills count as defence skills? How much smaller
must the loss be? The host comment seems to belong to a different point.

### US-020 Stats of the late-period officers

| Field | Value |
|---|---|
| Source | Slide 15, line 1 |
| Original text | 三國後期武將數值調整一下 |
| Host comment | none |
| Stars | 3 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a history fan, I want new stats for the officers of the late
period, so that the late scenarios agree with the history.

**Acceptance criteria**

- Given a late scenario, when I look at its best officers, then their
  stats are close to those of the best early officers.

**Open question.** What officers need new stats? Must the stats go up or
down? The card does not say.

### US-021 Decline of old officers

| Field | Value |
|---|---|
| Source | Slide 15, lines 2 and 3 |
| Original text | 衰老系统可以再调一下 / 前期武将活到后期大幅下降 |
| Host comment | none |
| Stars | 3 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a role-play player, I want an old officer to lose ability as
the years pass, so that the next generation takes over.

**Acceptance criteria**

- Given an early officer who lives into the late period, when the years
  pass, then the stats of the officer drop by a large amount.
- Given an old officer, when I open the panel of the officer, then the panel
  shows the drop in the stats.

**Open question.** At what age does the drop start? What stats drop?

### US-022 Faster growth of young officers

| Field | Value |
|---|---|
| Source | Slide 15, lines 4, 5 and 6 |
| Original text | 后期武将能力值提升明显一点 / 或者经验值增加显著一点 / 毕竟后期年轻人的天下 |
| Host comment | 或是經驗值增加顯著一些 |
| Stars | 3 |
| Underlined | Yes (line 5) |
| Scope | Beyond the original |

**Story.** As a role-play player, I want young officers to grow faster in
stats or experience, so that the late period belongs to the young.

**Acceptance criteria**

- Given a young officer of the late period, when the officer gets
  experience, then the stats rise faster than in RTK XI.

**Open question.** Must the growth come from higher stats, from more
experience, or from both?

## 5. People and life

### US-023 Search for officers among the people

| Field | Value |
|---|---|
| Source | Slide 8, lines 1 and 2 |
| Original text | 如果重置 / 比較想要把312的民間巡將 |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a role-play player, I want to search among the people for
officers, as in 三國志12, so that I can find new talent.

**Acceptance criteria**

- Given my city, when I order a search among the people, then the search
  can find an officer of no force.

**Open question.** The card gives only the name 民間巡將. What does this
feature of 三國志12 do?

### US-024 Officers have children

| Field | Value |
|---|---|
| Source | Slide 8, line 3 |
| Original text | 和313的小孩系統還原 |
| Host comment | 和313的小孩系統還原 |
| Stars | 2 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a role-play player, I want my officers to have children, as in
三國志13, so that my family continues.

**Acceptance criteria**

- Given a married officer, when the years pass, then the officer can have a
  child.
- Given a new child, when I open the panel of the parent, then the panel
  shows the child.

**Open question.** Can the player choose the spouse of an officer? Does the
character of the player also have children?

### US-025 Raise and train children

| Field | Value |
|---|---|
| Source | Slide 8, line 4 |
| Original text | 就像10代那樣生小孩培育呀 |
| Host comment | none |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a role-play player, I want to raise and train my children, as
in 三國志10, so that they become strong officers.

**Acceptance criteria**

- Given a child of my officer, when I choose how to train the child, then
  the choice changes the adult stats of the child.
- Given a child who reaches adult age, when the year starts, then the child
  becomes an officer of my force.

**Open question.** At what age does a child become an officer? What choices
of training does the player have?

### US-026 Behead a captive

| Field | Value |
|---|---|
| Source | Slide 9, line 1 |
| Original text | 俘虜後能斬首系統！！！ |
| Host comment | 對好玩，這不錯 |
| Stars | 2 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a role-play player, I want to behead an officer that I
capture, so that I can remove a dangerous enemy from the game.

**Acceptance criteria**

- Given an enemy officer in my prison, when I choose to behead the officer,
  then the officer dies and leaves the game.

**Open question.** Does the death of the captive change loyalty, the view of other
officers, or the relation with the enemy force? Check the captive rules of
RTK XI in the spec and record what the players want in addition.

### US-027 Kill an envoy

| Field | Value |
|---|---|
| Source | Slide 9, line 2 |
| Original text | 斬使者 |
| Host comment | 對好玩，這不錯 |
| Stars | 2 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a role-play player, I want to kill an envoy from an enemy, so
that I can show a clear refusal.

**Acceptance criteria**

- Given an envoy from an enemy force, when I choose to kill the envoy, then
  the envoy dies and the proposal ends.

**Open question.** What does the enemy force do after the death of its
envoy? Does the relation between the two forces change?

### US-028 Equipment stays with a dead officer

| Field | Value |
|---|---|
| Source | Slide 10, lines 1 and 2 |
| Original text | 新增： / 死掉的武將的裝備不掉落 |
| Host comment | none |
| Stars | 3 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a role-play player, I want a dead officer to keep the
equipment in the tomb, so that tombs hold old treasures.

**Acceptance criteria**

- Given an officer with a treasure, when the officer dies, then no force
  gets the treasure.
- Given a dead officer with a treasure, when I look at the tomb, then the
  tomb holds the treasure.

**Open question.** Does this apply to all equipment, or only to treasures?

### US-029 Dig up a tomb

| Field | Value |
|---|---|
| Source | Slide 10, line 3 |
| Original text | 需要去挖墓 |
| Host comment | 直接變古墓奇兵還什麼的 |
| Stars | 3 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a role-play player, I want to dig up a tomb, so that I can get
the equipment of a dead officer.

**Acceptance criteria**

- Given a tomb that holds a treasure, when my officer digs it up, then my
  force gets the treasure.

**Open question.** Where is the tomb on the map? Who can dig? Does a dig
change public order, loyalty or the view of other officers?

### US-030 Burial of a dead officer

| Field | Value |
|---|---|
| Source | Slide 10, lines 4 and 5 |
| Original text | 埋葬花錢加治安加忠誠 / 沒埋葬降士氣降治安 |
| Host comment | none |
| Stars | 3 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a role-play player, I want to pay for the burial of a dead
officer, so that public order and loyalty rise.

**Acceptance criteria**

- Given a dead officer of my force, when I pay for the burial, then my gold
  drops and public order and loyalty rise.
- Given a dead officer of my force, when I do not bury the officer, then
  morale and public order fall.

**Open question.** How long does the player have to decide? What cities and
what officers feel the effect?

## 6. Economy and population

### US-031 Population limit

| Field | Value |
|---|---|
| Source | Slide 11, line 1 |
| Original text | 人口限制 |
| Host comment | 人口限制徵兵限制 |
| Stars | 3 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a strategy player, I want each city to have a population with
a limit, so that a city cannot grow without end.

**Acceptance criteria**

- Given a city, when I open its panel, then the panel shows its population
  and its maximum population.
- Given a city at its maximum population, when a turn passes, then the
  population does not rise.

**Open question.** What makes the population rise or fall? War, food and
public order are possible causes.

### US-032 Conscription limit

| Field | Value |
|---|---|
| Source | Slide 11, line 1 |
| Original text | 徵兵限制 |
| Host comment | 人口限制徵兵限制 |
| Stars | 3 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a strategy player, I want conscription to have a limit, so
that a city cannot raise more troops than its people allow.

**Acceptance criteria**

- Given a city, when I order conscription, then the city cannot give more
  troops than its limit.
- Given a city at its limit, when I order conscription, then the order gives
  no troops.

**Open question.** Does the population set the limit? Does conscription
lower the population?

## 7. Weather and seasons

### US-033 Weather over the whole land

| Field | Value |
|---|---|
| Source | Slide 13, line 1 |
| Original text | 全國天氣效果 |
| Host comment | 天氣效果像冬天行軍難行 |
| Stars | 3 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a strategy player, I want the weather to change over the whole
land, so that the season matters to each campaign.

**Acceptance criteria**

- Given the map, when the weather changes, then the map shows the weather in
  each region.
- Given a region with bad weather, when a unit moves or fights there, then
  the weather changes the result.

**Open question.** What kinds of weather do the players want? Do the regions
of the north and the south have different weather?

### US-034 Hard marches in winter

| Field | Value |
|---|---|
| Source | Slide 13, line 1 |
| Original text | 好像冬天行軍難 |
| Host comment | 天氣效果像冬天行軍難行 |
| Stars | 3 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a strategy player, I want marches in winter to be harder, so
that I must time my campaigns to the seasons.

**Acceptance criteria**

- Given a unit in winter, when it moves, then it moves fewer hexes than the
  same unit in summer.

**Open question.** Does winter also cost troops or food? Does winter have
the same effect in the south?

## 8. The navy

### US-035 Classes of ships

| Field | Value |
|---|---|
| Source | Slide 14, line 1. Time 13:59. |
| Original text | 水戰可以把船隻細分？ |
| Host comment | none |
| Stars | none shown |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want several classes of ships, so that
the choice of ships matters in a battle on water.

**Acceptance criteria**

- Given a port of my force, when I build ships, then I can choose from more
  than one class.
- Given two classes of ships, when I compare them, then each class has
  different stats.

**Open question.** What classes do the players want? What makes the classes
different, for example size, speed or attack?

### US-036 Large ships for money

| Field | Value |
|---|---|
| Source | Slide 14, line 2. Time 13:59. |
| Original text | 北方可以靠財力建造大船 |
| Host comment | 感覺可以期待一下 |
| Stars | none shown |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a strategy player of a northern force, I want to build large
ships with money, so that I can match a southern navy.

**Acceptance criteria**

- Given a northern force with sufficient gold, when it orders a large ship,
  then a port of that force builds the ship.
- Given a large ship and a small ship, when they fight, then the large ship
  has the advantage.

**Open question.** Can every force build large ships, or only rich forces?
What advantage does the south keep?

## 9. Battle systems

### US-037 Two or three battle systems

| Field | Value |
|---|---|
| Source | Slide 17, line 1. Time 16:01. |
| Original text | 導入雙戰鬥系統/三戰鬥系統 |
| Host comment | 三國志10一般的戰鬥野戰攻城戰 |
| Stars | 5 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a strategy player, I want separate modes for field battles
and sieges, as in 三國志10, so that each has its own tactics.

**Acceptance criteria**

- Given two armies that meet in the field, when the battle starts, then the
  game opens a field battle mode.
- Given an army that attacks a city, when the battle starts, then the game
  opens a siege mode.
- Given a battle mode that ends, when I look at the strategy map, then the
  map shows the result of the battle.

**Open question.** Does "two systems" mean the map battle of RTK XI plus one
battle mode? Does "three systems" mean the map battle plus a field mode and
a siege mode? Can the player let the computer decide a battle?

## 10. Multiplayer

### US-038 Save an online game

| Field | Value |
|---|---|
| Source | Slide 16, line 1 |
| Original text | 聯機能存檔 |
| Host comment | none |
| Stars | 4 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As a multiplayer host, I want to save an online game, so that we
can stop and continue it later.

**Acceptance criteria**

- Given an online game, when I save it, then all players can load the same
  game later and continue.

**Open question.** Must all players be present to load the game?

### US-039 Rules of an online game

| Field | Value |
|---|---|
| Source | Slide 16, line 2 |
| Original text | 聯機可制定規則 |
| Host comment | 連線可以制定規則遊戲內能交流 |
| Stars | 4 |
| Underlined | Yes |
| Scope | Beyond the original |

**Story.** As a multiplayer host, I want to set the rules of an online game,
so that the game fits my group.

**Acceptance criteria**

- Given a new online game, when I open its settings, then I can set the
  rules before the start.
- Given a rule that I set, when a player tries an action that breaks it,
  then the game stops the action.

**Open question.** What rules do the players want to set?

### US-040 Messages inside the game

| Field | Value |
|---|---|
| Source | Slide 16, lines 3, 4 and 5 |
| Original text | 遊戲內能交流 / 可以賺多一點 / 而不是要讓玩家線上交流 |
| Host comment | 連線可以制定規則遊戲內能交流 |
| Stars | 4 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As an online player, I want to send messages inside the game,
so that we need no other program.

**Acceptance criteria**

- Given an online game, when I write a message, then the other players see
  it in the game.

**Open question.** Line 4 says that someone can earn a bit more. Does it
mean the publisher or the players? Line 5 can mean that players now talk
outside the game. The document reads the lines that way.

### US-041 Rank list for duels

| Field | Value |
|---|---|
| Source | Slide 16, line 6 |
| Original text | 單挑排位系統XD |
| Host comment | none |
| Stars | 4 |
| Underlined | No |
| Scope | Beyond the original |

**Story.** As an online player, I want a rank list for duels, so that I can
compare my duel results with other players.

**Acceptance criteria**

- Given a duel between two players, when the duel ends, then the rank list
  shows the new rank of each player.

**Open question.** The line ends with "XD". Is it a real wish or a joke?

### US-042 Online game with paid items (joke)

| Field | Value |
|---|---|
| Source | Slide 12, lines 1 and 2 |
| Original text | 不然把三國做成線上遊戲 / 搞成能課金的(XD) |
| Host comment | 但是我會想跟他說 |
| Stars | 3 |
| Underlined | No |
| Scope | Beyond the original |

The card marks this wish as a joke with "(XD)". The subtitle starts a reply
by the host, and the screenshot does not show the rest. This document keeps
the wish so that the list is complete. It is not a requirement until the
user confirms it.

**Story.** As a player, I want an online 三國志 with paid items, so that I
can pay to make my force stronger.

**Acceptance criteria**

- Given an online game, when a player pays real money, then the player gets
  an item in the game.

**Open question.** Does the user want to keep this wish, or drop it?
