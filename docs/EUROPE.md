# The European game

This document is the plan for the game of the year 1200 in the pack
data/medieval_europe. It covers the requests of the player of 2026-10-28.
It gives the design, the milestones E1 to E8 and the open questions.

The requests:
1. The game starts in English, not in Chinese.
2. Each ruler and each officer has a name in their own language, with the
   accents.
3. The flag of a unit shows the coat of arms of the faction, not the
   family name of the ruler.
4. Each officer has a short life and a link to the Wikipedia article.
5. A story mode tells why the war starts and what each ruler thinks.
6. The church takes the place of the Han emperor as the source of
   legitimate power. The player asked for five authorities, after the
   lectures of Dr. Roy Casagranda.
7. English keys identify the people, the factions and the cities. Big5
   bytes are only for the display of Chinese text.
8. The game of 1200 has two maps: the west and the east (E8).

## The rule of compatibility
Each change of this plan is a change of the pack, or a change that a pack
turns on. The Three Kingdoms game does not change.
- The placeholder pack keeps zh-TW as its default locale.
- The placeholder pack keeps its names files with the Big5 keys.
- A faction without a coat of arms keeps the flag with the family name.
- The rules of the Han emperor stay in code/sim/rivals.cpp. The pack
  rules choose the emperor or the church.
- The format of a save does not change. The English keys come from the
  TSV files of the pack, by the id of each record.
- The fixtures of spec-src/spec/fixtures stay green after each step.

## E1. The default locale of a pack
A pack can name its default locale in the file pack.tsv. The file has
the columns key and value:

    key	value
    default_locale	en

When the game starts the engine of a pack, it reads pack.tsv of that
pack. If the file names a locale, the game loads that locale. If the pack
has no pack.tsv, the locale stays as it is. The placeholder pack gets a
pack.tsv with zh-TW, so that a return to the Three Kingdoms gives the
Chinese UI again.

The player can change the locale in the Settings panel. The choice holds
until the game starts the engine of another pack.

Tests:
- A unit test of the text module reads pack.tsv through the test reader.
- A ui test chooses the 1200 scenario and checks the locale after the
  restart of the engine.

## E2. English keys and the names of a pack
The engine keeps names as Big5 bytes, because the save format of the
original game holds Big5 bytes. The pack files of 1200 already give an
English key to each record:
- officers.tsv: the column key, for example philip_ii;
- forces.tsv: the column key, for example france;
- map/source/sites.tsv: the column key of each city, pass and port, for
  example paris or brenner.

The design:
1. The engine reads the keys of the pack when it loads the pack scenario.
   It also reads them when it loads a save of the pack. The id of a record
   is the same in the TSV file and in the save.
2. engine_api gets three queries: officer_key, force_key and base_key.
   Each one gives the English key of an id, or "" if the record has no key.
   A record of an original save has no key.
3. The text module loads names/<locale>.tsv of the game pack, in addition
   to the names of the placeholder pack. The keys of that file are English
   keys with a prefix:

       officer.philip_ii	Philip II Augustus
       force.france	Kingdom of France
       base.paris	Paris

4. The UI asks for the name of the key first. If the record has no key, or
   the file has no row, the UI uses the Big5 bytes as now.

The Big5 bytes stay in the record. The zh-TW locale decodes them, as now.

## E3. Names in their own language
officers.tsv gets three new columns:
- native_lang: the ISO 639-1 code of the language, for example fr, de,
  es, ca, pt, it, ar, el, bg, hu, pl, cs, da or fa;
- name_native: the name in that language and its script, with the accents,
  for example "Philippe II Auguste" or "محمد الناصر";
- name_latin: the same name in the Latin script with diacritics, for
  example "Muḥammad al-Nāṣir". For a Latin script, it is a copy of
  name_native.

The source of each native name is the title of the Wikipedia article in
that language. The research checked each title through the MediaWiki API.
Three rows use the name of 1200, not the title of a later rank:
- Baldwin IX of Flanders is "Baudouin IX de Flandre", not the Latin emperor;
- Henry of Flanders is "Henri de Flandre";
- Ugolino of Segni is "Ugolino dei Conti di Segni", not Pope Gregory IX.

An officer of England has the English name of the game as the native name.

The display:
- A new choice in the Settings panel, "Names", has the values "English"
  and "Own language".
- "Own language" shows name_latin. The fonts of the game have the Latin
  letters with diacritics. They have no Arabic, Greek or Cyrillic
  glyphs yet.
- Arabic text needs a right-to-left layout and joined letter forms. Dear
  ImGui does not do this. Thus the UI does not show the Arabic script
  until a later step adds a shaper. See question Q-E4.

The implementation:
- misc/make_scenario.py writes names/native.tsv of the pack. The row
  officer.<key> has the name_latin of the officer. The tool stops if a
  name_latin is empty or has a letter that is not Latin.
- The text module loads names/native.tsv of the game pack with the names of
  the locale. The table does not depend on the locale.
- The field native_names of text_catalog is the Names setting. If it is on,
  text_key_name looks in the native table first. A key that the native table
  lacks gives the normal name. The setting loads no file.
- The Settings panel has the combo Names. The combo sets native_names and
  commands nothing in the engine. The setting stays when the locale changes.
- The CJK font has no glyph for six letters of the native names. These
  letters are "ł", "ř", "ʿ", "ḥ", "ṣ" and "ẓ". The loader merges Noto Sans
  (SIL OFL 1.1) for the Latin Extended ranges. docs/ASSETS.md lists the font.
  The web build packs the font.

Tests:
- unit_text_native_name_tests checks the table, the setting, the fallback
  to the normal name and a pack without native.tsv.
- The ui test "names_choice_of_the_settings_panel_shows_the_native_name_of_an_officer_and_commands_nothing"
  chooses "Own language" and "English", checks the name of an officer with
  a key, and checks that the choice commands nothing.

## E4. The life of each officer
Each of the 103 officers gets a short life of three or four sentences and
the URL of the English Wikipedia article.
- The URL goes in the new column wikipedia of officers.tsv.
- The life goes in lore/<locale>.tsv of the pack, with the key
  officer.<key>. A second locale can translate it later.
- The research wrote each life in its own words. It copied no sentence of
  a source. It checked each life against the Wikipedia article through the
  MediaWiki API. A second published source is still to check for each
  life. See "The data to check".
- The officer panel shows the life under the abilities. A button opens
  the Wikipedia article. On Windows, the platform opens the link in the
  default browser. On the web, the page opens it in a new tab.

The display:
- The panel is 武將情報, in the function ui_draw_officer_detail. The life
  is a wrapped text of 28 times the height of the font, under the skill line.
- The button "Wikipedia" is after the life. Its text id is officer.wikipedia.
- An officer without a life shows no text. An officer without a link
  shows no button. The Three Kingdoms pack has no lives, so its panel does
  not change.
- The button calls the platform and commands nothing in the engine.

The implementation:
- The UI does not read officers.tsv. misc/make_scenario.py writes
  lore/links.tsv of the pack. The row officer.<key> has the URL. An officer
  with "-" has no row.
- The text module loads lore/<locale>.tsv and lore/links.tsv of the game
  pack. text_key_lore gives the life. text_key_link gives the URL.
- If the locale has no life for a key, text_key_lore gives the life of the
  fallback locale (English). The links table does not depend on the locale.
- The platform has a new function, open_url in platform_api
  (code/hexdom.h). The function gives false if it did not open the link.
- text_link_is_allowed accepts a URL only if it starts with
  https://en.wikipedia.org/wiki/ and has a name after the prefix. The URL
  must have no space and no control character. It must also have none of
  the characters " < > \ ^ ` { | }. The UI and each platform call the
  function, so a bad link never reaches the system.
- win32_open_url converts the URL from UTF-8 and calls ShellExecuteW. The
  link of a Wikipedia article can have letters outside ASCII. build.bat
  links shell32.lib for this call.
- wasm_open_url calls window.open with the target _blank and the option
  noopener. The game runs in the frame loop, not in a handler of a click of
  the page. A browser can thus block the new tab. No test checks this yet.
- The font loader scans lore/<locale>.tsv for glyphs, as it does for
  names/<locale>.tsv.
- The headless tests and demo_player give no open_url. In that case the
  panel shows no button.

Tests:
- A tool check in misc/make_scenario.py makes sure that each officer has
  a life and that each wikipedia cell is "-" or an allowed URL. The check
  uses the same rule as text_link_is_allowed.
- unit_text_lore_tests checks the lives, the fallback locale, the links
  table and a pack without lore. unit_text_link_tests checks the allowed
  and the refused URLs.
- The ui test "officer_panel_shows_the_life_and_the_wikipedia_button_opens_the_link_and_commands_nothing"
  opens the panel and checks the life. It clicks the button and checks the
  URL that the platform gets. It also checks that nothing goes to the engine.
- The ui test "officer_panel_shows_no_button_without_a_link_and_no_text_without_a_life"
  checks an officer with a life and no link, an officer without a key, and
  the Three Kingdoms pack.

## E5. The coat of arms of each faction
The flag of a unit shows the coat of arms of the faction. The arms also
show in the faction list and in the ruler list of a new game.

The design:
1. arms.tsv of the pack lists one image for each force key. The images
   are PNG files in data/medieval_europe/arms. A tool turns the SVG files
   of Wikimedia Commons into PNG files of 128 x 128 pixels.
2. The platform makes a texture from each PNG file, as it does for the
   portraits.
3. ui_draw_unit_flag draws the arms on the cloth of the flag. A faction
   without arms keeps the family name of its ruler.

The licences:
- Many images on Wikimedia Commons are in the public domain. Others have
  the licence CC BY-SA. A CC BY-SA image needs the name of the author and
  the licence in the credits. A change to the image must keep the same
  licence.
- docs/ASSETS.md and code/ui/ui_credits.cpp list each image, its author,
  its licence and its URL.
- The plan uses only public-domain images until the player agrees to CC
  BY-SA. See question Q-E3.

The research of 2026-10-28 chose one SVG file on Wikimedia Commons for
each faction. It read the licence of each file through the Commons API.
It downloaded no image.

docs/europe/ARMS.md gives the list with a reference article, the page on
Commons and the public-domain choices. docs/europe/arms.tsv gives the
direct URL of each file. The external work makes the images from this
list and puts them in a shared Drive folder. ARMS.md gives the names and
the formats of these files.

| Force | Key | File on Commons | Licence | Public-domain choice |
|---|---|---|---|---|
| 0 | france | Blason pays fr France ancien.svg | Public domain | - |
| 1 | angevin | Coa England Country History Anjou-Plantagenêt (1198-1340).svg | CC0 | - |
| 2 | toulouse | Blason département fr Hérault.svg (the cross of Toulouse) | Public domain | - |
| 3 | aragon | Coa Spain Family Royal arms of Aragon.svg | CC0 | - |
| 4 | castile | Escudo de armas de Castilla.svg | CC0 | - |
| 5 | leon | Shield of Alfonso IX of Leon.svg | CC BY-SA 3.0 | Kingdom of Leon Medieval Heraldic Lion.svg |
| 6 | portugal | CoA of Portugal (1139-1247).svg | CC BY-SA 3.0 | Banner of Portugal 1248.svg |
| 7 | almohad | Almohad flag.svg | CC0 | - |
| 8 | ghaniya | Banner of the Almoravids.svg | Public domain | - |
| 9 | ayyubid | Flag of Ayyubid Dynasty.svg (Commons marks it for deletion) | Public domain | - |
| 10 | byzantium | Byzantine imperial flag, 14th century, square.svg | Public domain | - |
| 11 | bulgaria | Coat of arms of the Second Bulgarian Empire.svg | CC BY-SA 3.0 | none found |
| 12 | rum | Double-headed eagle of the Sultanate of Rum.svg | CC0 | - |
| 13 | staufer | Armoiries empereurs Hohenstaufen (DL).svg | Attribution | none found |
| 14 | welf | Emperor Otto IV Arms (DL).svg | Attribution | none found |
| 15 | sicily | Coat of Arms of the Kingdom of Sicily (House of Hohenstaufen).svg | CC BY-SA 4.0 | none found |
| 16 | austria | CoA Babenberg (old).svg | CC0 | - |
| 17 | bohemia | Přemyslovci erb.svg | Public domain | - |
| 18 | hungary | Coa Hungary Country History Imre (1196-1204).svg | Public domain | - |
| 19 | poland | POL Przemysł II 1295 COA.svg | Public domain | - |
| 20 | papacy | Emblem of the Papacy SE.svg | Public domain | - |
| 21 | venice | Flag of Most Serene Republic of Venice.svg | Public domain | - |
| 22 | flanders | Arms of Flanders.svg | CC0 | - |
| 23 | denmark | Royal Banner of Denmark (14th Century).svg | Public domain | - |

The result:
- 18 factions have a public-domain or CC0 image.
- Leon and Portugal have a CC BY-SA image and a public-domain choice.
- Staufer and Welf need the credit "David Liuzzo" under an attribution
  licence.
- Bulgaria and Sicily have only CC BY-SA images.
- The arms of Byzantium, Bulgaria, Rum, Sicily, Poland, the papacy and
  Venice are later attributions, not arms of 1200.

Some factions of 1200 had no coat of arms. Heraldry started in western
Europe in the 12th century. The Almohads, the Ayyubids, Byzantium and the
Seljuks used banners and seals. For these factions, the plan uses a banner
of the period, or a later attribution with a note.

## E6. The authorities of the church
This section is a design. On 2026-10-28 the player chose the Pentarchy
as the five authorities (Q-E1). The player also chose a new city for
Antioch (Q-E7). On 2026-10-29 the player chose two caliphs as the
authorities of the Muslim factions (Q-E2). The stage e6_church
(misc/stages/e6_church.md) does the work.

In the Three Kingdoms game, the Han emperor gives legitimacy. The faction
that holds his city gains technique points each month. Officers who
respect the Han gain loyalty. The rank of emperor needs the protection of
the emperor.

In 1200, the church gave legitimacy to the kings of Christian Europe. The
pope crowned kings and released subjects from their oaths. Five sees had
the rank of patriarch since the Council of Chalcedon in 451. Historians
call this group the Pentarchy:
1. Rome, the see of the pope;
2. Constantinople;
3. Alexandria;
4. Antioch;
5. Jerusalem.

Dr. Casagranda tells the same five in his lecture "The Crusades: Part 1"
of 27 March 2023, near minute 53. In his words, the old church had two
popes, two patriarchs and an archbishop. The popes sat in Rome and in
Alexandria. The patriarchs sat in Antioch and in Constantinople. The
archbishop sat in Jerusalem. In his account, the pope of Rome did not
accept a rank of one in five. The quarrel grew into the split of the East
and the West. Other sources correct two details of the lecture:
- The split has the date 1054, not 1058.
- The see of Jerusalem had the rank of patriarch from 451.

The holders of the five sees on 1 January 1200:

| See | Latin holder | Greek or Coptic holder | City on the map | Owner in 1200 |
|---|---|---|---|---|
| Rome | Innocent III, 1198 to 1216 | none | Rome, 20 | papacy |
| Constantinople | none until Thomas Morosini, 1204 | John X Kamateros, 1198 to 1206 | Constantinople, 15 | byzantium |
| Alexandria | none | Mark III, 1180 to 1209. Coptic pope John VI, 1189 to 1216, in Cairo | Alexandria, 12 | ayyubid |
| Antioch | Peter of Angouleme, 1196 to 1208 | in exile in Constantinople | Antioch, a new city | Bohemond III, not a faction |
| Jerusalem | Aymar the Monk, 1194 to 1202, in Acre | Mark II, uncertain | Jerusalem, 14 | ayyubid |

The Muslim rulers took their legitimacy from a caliph. The rules of the
caliph are not the rules of the church:
- The Ayyubids named the Abbasid caliph al-Nasir li-Din Allah in the
  Friday sermon and on their coins. He ruled in Baghdad from 1180 to 1225.
  Baghdad is a city of the east map, and the Abbasid faction holds it
  (E8).
- The Almohads did not accept Baghdad. Their own ruler had the title of
  caliph.
- The Banu Ghaniya were heirs of the Almoravids. The Almoravids accepted
  the caliph of Baghdad in name.
- The Seljuks of Rum received envoys of the caliph. The research found no
  diploma of the caliph to Rum near 1200.

Dated facts for the rules:
- 1179: the bull Manifestis Probatum made Afonso Henriques king of
  Portugal. Portugal paid the pope each year.
- 1198: Innocent III put Leon under interdict. He did not accept the
  marriage of Alfonso IX and Berenguela.
- January 1200: Innocent III put France under interdict. Philip II did not
  accept his wife Ingeborg. The pope stopped the interdict in September.
- 1201 and 1202: Innocent III chose Otto IV as king of Germany. The
  decretal Venerabilem of 1202 claimed the right of the pope to judge
  between two kings.
- 8 November 1204: a legate of the pope crowned Kaloyan at Tarnovo. The
  pope gave the title of king, not of emperor.
- 11 November 1204: Innocent III crowned Peter II of Aragon in Rome.
- 1208 to 1214: England was under interdict. The pope excommunicated John
  in 1209. In 1213 John made England a fief of the pope.
- 1209 and 1210: Innocent III crowned Otto IV emperor, then excommunicated
  him one year later.

The proposed rules:
- authorities.tsv of the pack lists the five sees: a key, the city of the
  see, the holder in 1200 and the church of the holder.
- rules.tsv gets legitimacy.model with the values emperor and
  authorities. The placeholder pack keeps emperor.
- Each month, the faction that holds the city of a see gains technique
  points. Devout officers of that faction gain loyalty. These are the
  rules of the emperor, for each see.
- The rank of king or emperor needs the recognition of one authority.
  A faction gets recognition if it holds the see, or if the holder of the
  see is its ally.
- A new command lets a faction ask the pope for a crown. The pope gives
  it to a faction with a high friendliness. Kaloyan of Bulgaria got his
  crown from Innocent III in 1204. Innocent III crowned Peter II of Aragon
  in Rome in 1204.
- The pope can excommunicate a ruler or put a land under interdict.
  Innocent III put France under interdict in 1200. Then the officers of
  that faction lose loyalty each month.

The facts of this section come from sources that the research checked.
The list of sources is in the section "The sources".

### The two caliphs
The player chose two caliphs on 2026-10-29 (Q-E2). The game then has
seven authorities: the five sees and two caliphs.

| Authority | Seat | Map of the seat | The factions that it recognizes |
|---|---|---|---|
| The Abbasid caliph | Baghdad | east | ayyubid, rum, ghaniya, and the Mamluks after E7 |
| The Almohad caliph | Marrakesh | west | almohad |

The rules of a caliph are the rules of a see:
- Each month, the faction that holds the seat gains technique points.
  Devout officers of that faction gain loyalty.
- A Muslim ruler needs the recognition of his caliph for the rank of
  king or emperor.
- If the seat is not on the map, the caliph acts without a city. A
  faction then gets recognition only from friendliness with the holder.

The reasons for two caliphs:
- The Almohad rulers had the title of caliph and did not accept Baghdad.
- The Ayyubids named the Abbasid caliph in the Friday sermon.
- In 1261 Baybars put an Abbasid caliph in Cairo after the Mongols took
  Baghdad in 1258. Thus a caliph can move to a new seat. The story of the
  Mamluks uses this fact.

### The city of Antioch
The player chose to add Antioch as a city (Q-E7). Antioch is at
36.20 N, 36.16 E. The east map of E8 holds it as one of its 42 cities.
The map of the prototype does not change, and no city leaves a map.

The five sees on the two maps:
- The east map holds all five sees: Rome, Constantinople, Alexandria,
  Antioch and Jerusalem.
- The west map holds only Rome. The other four sees act there without a
  city.

In 1200, Bohemond III held the principality of Antioch, from 1163 to
1201. The east map can make the principality a faction. The see of
Antioch has a Latin patriarch in the city and a Greek patriarch in
exile.

## E7. The story mode
The story mode tells the background of the war before the first turn. It
is a series of pages with text and a map.

The pages:
1. Europe in 1200. The page shows these powers:
   - the Angevin lands in France and the Reconquista in Spain;
   - the Almohads in Africa and the heirs of Saladin in Egypt and Syria;
   - the weak Byzantine Empire and the two kings of Germany.
2. The authorities. The pope Innocent III and his claim over the kings.
   The five sees and the split of 1054 between Rome and Constantinople.
3. The sparks of the war. The interdict on France in 1200. The death of
   Richard I in 1199 and the claim of Arthur of Brittany. The call of a new
   crusade in 1198. The double election in Germany in 1198.
4. One page for each ruler. What the ruler wants, whom the ruler fears,
   and the first goal of the ruler in 1200.

The story mode is a front-end feature. It reads story/<locale>.tsv of the
pack and draws the pages. It sends no command to the engine.

The stage e7_story built the story mode:
- Each pack has story/en.tsv. The rows are title.<page> and page.<page>
  for the three pages of the world, and force.<key> for each faction.
- The text is in the Latin script (Q-E4). A locale without its own file
  uses the English file, as the lore does. Thus there is no zh-TW file.
- The text module has text_key_story. The panel is code/ui/ui_story.cpp.
  It has the buttons Previous, Next and Skip.
- The new-game screen shows the Story button only if the pack has story
  pages. The panel asks the engine only for the keys of the factions.
- The pages are in the words of the research. Each fact has two sources in
  docs/europe/SOURCES.md.

### The events during the game
The player chose events during the game (Q-E5). An event does not start
on a fixed date. Each event has a window of years and preconditions on
the state of the game. If the game goes another way than history, the
event does not start, or it starts later.

The engine has the interpreter of these events. history.tsv of a pack
gives the rows, and code/sim/history.cpp checks them on day 1 of each
month (M9). A row starts one time in a game. The interpreter has these
conditions now:
- date_from: the first month of the window;
- rules, rules_any and not_rules: the faction holds the cities, or not;
- ruler, faction_of, has, dead and death_year: the state of an officer;
- cities, rank and officers: the size of the faction;
- loyalty_below and merit_at_least: the state of an officer.

The events of 1200 need these new conditions:
- date_until: the last month of the window;
- at_war and allied: the relation of two factions;
- owner: the owner of a city is in a list of factions, or the city has
  no owner;
- church: the church of the faction is latin, greek or muslim;
- after: the event can start only after another event.

Each row already has the column chance, a percent for each month. Thus
two games do not have the same date, and no new clause is necessary.

The rows of the Three Kingdoms game name an officer by the Big5 bytes.
The rows of the Europe pack name an officer by the English key, for
example @baybars.

The events of the story:

| Event | History | Window | Preconditions | Effects |
|---|---|---|---|---|
| The Fourth Crusade | The army sailed from Venice in 1202 | 1201 to 1210 | Innocent III is alive. Venice is not at war with France or Flanders. | Officers of France, Flanders and Venice gain merit. Venice gains gold. |
| The sack of Constantinople | The crusaders took the city in April 1204 | 1203 to 1212 | After the Fourth Crusade. Byzantium holds Constantinople. Its ruler has low loyalty in his city. | Byzantium loses order and loyalty. The Latin patriarch of Constantinople takes the see. |
| The Albigensian Crusade | Innocent III called it in 1208 | 1205 to 1220 | Raymond VI rules Toulouse. The papacy has low friendliness with Toulouse. | Toulouse loses loyalty. France gains friendliness with the papacy. |
| Las Navas de Tolosa | Castile, Aragon and Navarre beat the Almohads in July 1212 | 1208 to 1225 | Castile and Aragon are allies. Castile is at war with the Almohads. The Almohads hold Seville. | The Almohads lose morale. The Christian factions of Iberia gain merit. |
| Bouvines | Philip II beat Otto IV and Ferrand of Flanders in July 1214 | 1210 to 1225 | France is at war with the Welf party and with the Angevins. | The winner of the next battle gains rank. |
| The Mamluks take Cairo | The mamluk generals killed Turanshah in 1250 | 1240 to 1270 | The Ayyubids hold Cairo. The ruler is dead, or a mamluk officer has high merit and low loyalty. | A mamluk officer takes the rule. The faction becomes the Mamluk Sultanate. |
| The siege of Antioch | Baybars took Antioch on 18 May 1268 | 1260 to 1290 | Baybars rules the faction of Cairo. A latin faction holds Antioch, or the city has no owner. | The AI of Baybars marches on Antioch. If he takes it, the city loses walls and people. The Latin patriarch leaves the see. |

### The story of the Mamluks
The player asked for the story of the Mamluk sultan Baybars and the
siege of Antioch in 1268. Wikipedia tells the history in the article
"Siege of Antioch (1268)":
https://en.wikipedia.org/wiki/Siege_of_Antioch_(1268)

The story has these parts:
1. New officers appear. Qutuz, Aybak, Shajar al-Durr and Baybars join
   officers.tsv as free officers with a year of appearance. Baybars was
   born approximately 1223. He appears in Egypt approximately 1240.
2. The Mamluks take Cairo. The event of the table gives the rule to a
   mamluk officer. The force keeps its id. Its English name changes.
3. Baybars becomes sultan. Historically, Baybars took the rule after the
   battle of Ain Jalut in 1260. The event needs Baybars in the faction
   with high merit.
4. The siege of Antioch. The event of the table starts the march.
5. The fall of Antioch. The see of Antioch loses its Latin patriarch
   (E6). The Christian factions lose friendliness with the Mamluks. The
   papacy can call a new crusade. Historically, Louis IX started the
   Eighth Crusade in 1270.

The new effects are these:
- seize: an officer takes the rule of his faction;
- rename: the faction gets a new English name;
- target: the AI of the faction marches on a city;
- city_loss: a city loses a percent of its walls and its troops;
- see_holder: a see of E6 gets a new holder, or no holder.

The stage e7_story (misc/stages/e7_story.md) adds all of these effects
except see_holder. E6 adds see_holder and the parts of the events that
touch a see.

Each new condition and each new effect gets a unit test first in
tests/unit/playable_tests.cpp, beside the tests of the M9 events. The Three Kingdoms events do not use
them, so the fixtures stay green.

### The rows of the stage
The M9 effects have no gold. Thus the row of the Fourth Crusade gives
public order to the crusaders in place of gold to Venice. The rows leave
out the sees. The Latin patriarch of Constantinople waits for E6.

The east map has six rows: fourth_crusade, sack_of_constantinople,
mamluks_take_cairo, baybars_sultan, siege_of_antioch and fall_of_antioch.
The west map has four rows: albigensian_crusade, albigensian_france,
las_navas_de_tolosa and bouvines. The tests of both lists are in
tests/unit/story_tests.cpp.

The engine changes each @key of a row into an officer before the sim reads
the row. The file engine_history_keys.cpp does this. The rename effect keeps
a key, and the sim derives the name of the faction from the rows that
started. Thus the save format does not change, and a saved game keeps the
name of the faction after a load. The engine fills the church of each
faction from forces.tsv, because the save does not hold it. The unit tests
check each row and each clause. The feature test of the pack checks the
rename after a save and a load.

## E8. The west map and the east map
On 2026-10-28 the player chose two maps for the game of 1200. The west
map has the war of England and France at its centre. The east map has the
crusades at its centre. Each map is a pack of its own, with its own 42
cities. Thus each map has approximately two times the cities of the
prototype in each region.

| Map | Pack | Box | Hexes | The wars |
|---|---|---|---|---|
| West | medieval_west | 10.5 W to 15 E, 30.5 N to 56 N | 268 x 286 = 76648, 10.1 km | England and France, the two kings of Germany, the Reconquista, the Albigensian Crusade |
| East | medieval_east | 8 E to 46 E, 28.5 N to 49 N | 366 x 217 = 79422, 10.7 km | the crusades in Syria and Egypt, the Fourth Crusade, Byzantium and Bulgaria, the Ayyubids and the Mamluks |

misc/make_world_map.py sets the size of a hex from the distance between
the cities. The limit of 80000 hexes stops a finer scale on the east map.
There the median distance to the nearest city is 17 hexes, not 22.

### The reasons for the line
The east edge of the west map is 15 E, not the Rhine:
- At Bouvines in July 1214, Philip II beat a coalition. John of England,
  the emperor Otto IV, Ferrand of Flanders and Renaud of Boulogne were in
  this coalition.
- Otto IV had his base in Cologne and in Brunswick. Philip of Swabia, his
  rival of the Staufer party, was the ally of Philip II.
- The pope judged the kings of the west. Innocent III put France under
  interdict in 1200 and England in 1208. Rome is at 12.5 E.

Spain and Morocco are on the west map:
- Peter II of Aragon died at Muret in September 1213. He fought for
  Toulouse against the crusaders of Simon de Montfort.
- Alfonso VIII of Castile went into Gascony in 1205. He claimed it as the
  dowry of his wife Eleanor, the sister of John.
- The Almohads did not accept the caliph of Baghdad. Saladin asked them
  for a fleet, and no fleet came. Their war with the kings of Iberia was a
  war of the west.

Thus the west map also has a war of Christians and Muslims. Las Navas de
Tolosa in 1212 is an event of the west map.

The east map goes from Italy to Baghdad:
- It holds the five sees of E6.
- Baghdad is a city of the map. The caliph can then be an authority with
  a city (Q-E2).
- The Fourth Crusade sails from Venice to Constantinople inside the map.
- The story of the Mamluks goes from Cairo to Antioch.
- Later events can come in from the east edge, for example the Mongols
  after 1240.

### The parts in the two maps
The boxes overlap from 8 E to 15 E. Italy, Sicily, Corsica, Sardinia,
Tunis and Tripoli are on both maps. The papacy, Venice and Sicily are
factions of both maps. Each map is a game of its own, so the overlap is
not a problem.

Poland is on neither map. Krakow is at 50.06 N, north of the east map.

The factions of the prototype on each map:

| Map | Factions |
|---|---|
| West | france, angevin, toulouse, aragon, castile, leon, portugal, almohad, ghaniya, staufer, welf, sicily, bohemia, papacy, venice, flanders, denmark |
| East | byzantium, bulgaria, rum, ayyubid, hungary, austria, papacy, venice, sicily, almohad, ghaniya, jerusalem, antioch, cilicia, cyprus, georgia, abbasid |
| Neither | poland |

The stage e8_factions added six factions to the east map:
- the kingdom of Jerusalem, with Acre;
- the principality of Antioch;
- the Armenian kingdom of Cilicia;
- the kingdom of Cyprus;
- the kingdom of Georgia, with Tbilisi in the place of Bologna;
- the Abbasid caliphate, with Baghdad.

The west map can add the kingdom of Navarre.

### The result of E8
Each pack has 42 cities, 10 passes and 35 ports. Each pack has a scenario of 1200.

| Map | Cities that are new | Factions | Officers |
|---|---|---|---|
| West | Rouen, Chinon, Lille, Zaragoza, Reims, Coimbra, Cordoba, Brunswick, Roskilde, Genoa, Burgos, Santiago | 17 | 77, with 8 free |
| East | Antioch, Aleppo, Edessa, Mosul, Baghdad, Damascus, Acre, Nicosia, and 16 more in Anatolia, the Balkans and Italy | 17 | 65, with 6 free |

The notes of the result:
- Antioch is a city of the east map. The principality of Antioch owns it. Aleppo and Damascus belong to the Ayyubids.
- The six factions of step 4 have the force ids 11 to 16. The order is Jerusalem, Antioch, Cilicia, Cyprus, Georgia and the Abbasid caliphate.
- The cities of the six factions are Acre, Antioch, Sis, Nicosia, Tbilisi and Baghdad.
- Tbilisi is city 19 of the east map. It takes the place of Bologna.
- The east map has 6 cities without an owner: Barqa, Milan, Genoa, Florence, Belgrade and Mosul.
- Aimery of Lusignan is the king of Jerusalem and of Cyprus. He governs Acre. The officer Hugh I of Cyprus governs Nicosia.
- Bohemond IV was count of Tripoli in 1200. The pack puts him at Antioch, the city of his father. Raymond-Roupen is an infant. The pack puts him in Cilicia, the land of his grandfather.
- The new officers have the ids 43 to 64. Each has a life, a native name and a URL.
- Walter of Montbéliard came to Cyprus in 1202. The sim skips an officer that appears after the year of the scenario, so his year of appearance is 1200.
- The rows 11 to 23 of faction_colours.tsv have new colours for the new forces. The unused colours moved to the later rows.
- The Almohad officers stand in Tunis on the east map, because Marrakesh is not on the map.
- Rouen and Acre were ports in the prototype. They are cities now.
- The new game screen lists the games as rows 9 and 10 of scenarios.tsv, with the keys y1200_west and y1200_east.
- build_web.bat preloads both packs. Each pack adds approximately 3 MB to index.data.

### The work of E8
1. Make data/medieval_west and data/medieval_east from a copy of
   data/medieval_europe. Each pack gets its own map/source/region.tsv
   and map/source/sites.tsv.
2. Choose the 42 cities of each map. Use the seats of the factions, the
   fronts of the wars, and the populations of Bosker, Buringh and van
   Zanden (docs/MAPS.md).
3. Copy the officers of the factions of each map. The English keys stay
   the same in both packs. Thus the lives, the native names and the
   Wikipedia URLs stay the same.
4. Research the officers of the new factions, with two sources for each
   life (docs/europe/SOURCES.md). The stage e8_factions
   (misc/stages/e8_factions.md) did this step. It also put Tbilisi in
   the place of Bologna, for Georgia. The stage e8_maps
   (misc/stages/e8_maps.md) did the other steps.
5. Add one row for each pack to scenarios.tsv of the placeholder pack.
   The new game screen then lists the two games of 1200.
6. Add a feature test for each pack, as the test of the prototype.

data/medieval_europe stays as the prototype. Its feature test and its
saves do not change.

## The sources
The player likes the lectures of Dr. Roy Casagranda. The plan uses them as
a source of ideas and of the frame of the story. The plan does not copy
his words, because the lectures are under copyright.

Each fact of the game must have two sources or more, for example:
- the English Wikipedia article of the person;
- Encyclopaedia Britannica;
- a published history, for example the works on Innocent III.

## The data to check
The check of 2026-10-28 compared each life and each year with a second source. The file docs/europe/SOURCES.md lists the sources of each officer. The game data changed where two sources agree. The differences below stay open. The game data does not change until two sources agree.

Years that the sources give in different ways:
- 6 Montfort: born near 1165 or near 1175. The data has 1175.
- 14 Longespee: one source says born in 1167 or before. The English article says 1176. The data has 1176.
- 34 Al-Nasir: born in 1181 or in 1182. The data has 1182.
- 46 Laskaris: born in 1174 or in 1175. The data has 1174.
- 52 Suleiman II: born in 1158, 1160 or 1166. The data has 1160.
- 54 Philip of Swabia: born in 1177 or in 1178. The data has 1177.
- 55 Kalden: born near 1175 or near 1145. The data has 1150.
- 80 Ugolino: born near 1145 in the Catholic Encyclopedia, and near 1170 in Treccani. The data has 1145.
- 82 Ranieri Dandolo: died in 1208 or in 1209. The data has 1209.
- 83 Ziani: the English article says died on 13 March 1230. Treccani and one other source say 1229. The data has 1229.
- 86 Henry of Flanders: born in 1174, 1176 or 1178. The data has 1176.
- 91 Sunesen: born near 1165 in two Danish sources. The data has 1167.
- 94 Kaykhusraw I: born in 1169, 1171 or 1177. The data has 1164.
- 20 Peter II: some sources say that Innocent III crowned him in 1204, and others say 1205. The life says 1204.

Years that have no source or only one source:
- 18 Foix, 36 Abu Hafs, 50 Boril, 51 Strez and 95 Sgouros: no source gives a birth year. The sources for Sgouros say only "before 1170".
- 22 Montcada: the sources say only "12th century". The year 1150 is improbable.
- 57 Ludolf: Deutsche Biographie says born near 1125. The data has 1140.
- 64 Pagliara, 79 Capuano and 87 Bethune: the sources give only a limit, for example "before 1160".

Lives that have a weak second source:
- 35 Ibn Jami: only the article of al-Nasir names him. He was vizier from approximately 1205, not in 1200.
- 38 Yahya ibn Ghaniya: the English article of that name names another man. The officer has no English article.
- 57 Ludolf, 60 Henry of the Palatinate, 61 Henry of Brabant and 64 Pagliara: one source only.
- The "SOURCES.md" file marks the other weak sources.

The officers of the east factions (ids 43 to 64 of data/medieval_east) have these open points. The first source of each is an English article.

Years that the sources give in different ways:
- 43 Aimery: born in 1144 or in 1145. Wikidata says 1152. The data has 1144.
- 44 Isabella: born in 1169 or in 1172. The data has 1172.
- 47 Bohemond III: born in 1144, in 1145 or near 1148. The data has 1144.
- 48 Bohemond IV: born in 1172 or near 1175. The data has 1172.
- 50 Leo I: crowned in 1198 or in 1199. The life says both.
- 52 Raymond-Roupen: born in 1198 or in 1199, and died in 1219, 1221 or 1222. The data has 1198 and 1222.
- 53 Hugh: born in 1194, 1195 or near 1196. The English article says he was nine in 1205. The data has 1195.
- 54 Philip of Ibelin: died in 1227 or in 1228. The data has 1227.
- 57 David Soslan: married Tamar in 1189 or in 1191. The life says approximately 1189.
- 63 Ibn al-Jawzi: born in 1116 or in 1126, and died in 1200 or in 1201. The data has 1116 and 1201.

Years that are an estimate, because no source gives a year:
- 46 Geoffroy de Donjon, 49 Peter of Angoulême and 51 Gregory VI: born 1150. Wikidata gives 1200 for the first two. That is a placeholder.
- 56 Tamar: born near 1160 or near 1166. The data has 1160.
- 57 David Soslan: born 1160. 58 Zakare II: born 1160. 59 Ivane I: born 1165.
- 60 Rustaveli: born 1165 and died 1220. Both years are estimates.
- 55 Walter of Montbéliard: the year of appearance is 1200, but he came to Cyprus in 1202. The sim skips an officer that appears after the year of the scenario.

Lives that have a weak second source: 49, 51, 53, 54, 55, 57, 58, 59, 62 and 64. The file docs/europe/SOURCES.md has the list.

Other points of the east factions:
- Aimery of Lusignan is king of Jerusalem and of Cyprus. The two sources are Wikipedia and Larousse. Acre has Aimery, and Nicosia has Hugh. The scenario files cannot say that two factions are allies. The engine has only the liked and the disliked officers. Thus the alliance of Jerusalem and Cyprus is not in the data.
- 48 Bohemond IV was count of Tripoli in 1200. The data puts him at Antioch.
- 52 Raymond-Roupen was a small child. The data puts him in Cilicia.
- The arms of Jerusalem, Antioch, Cilicia, Cyprus and Georgia have no checked date (docs/europe/ARMS.md). The arms of Cyprus that are public domain are a photograph of a later relief.

Ten officers of the prototype have no English article. They have "-" in the column
wikipedia: 3, 4, 19, 22, 33, 35, 38, 57, 78 and 82.

## The milestones
| Step | Work | Needs an answer | Status |
|---|---|---|---|
| E1 | pack.tsv and the default locale | no | done |
| E2 | English keys in engine_api, names of the pack | no | done |
| E3 | Native names in officers.tsv and the Names setting | Q-E4 for Arabic | done, Latin script only |
| E4 | Lives and Wikipedia URLs, the officer panel | no | done |
| E5 | Coats of arms on the flags | Q-E3 | list done in docs/europe/ARMS.md, external art |
| E6 | The authorities of the church and the two caliphs | Q-E2, answered | brief in misc/stages/e6_church.md |
| E7 | The story mode and the events during the game | no | done, except the sees (E6) |
| E8 | The west map and the east map, with Antioch on the east map | no | done. The east map has 17 factions |

## The stage runs
misc/run_stages.py runs the stages as headless Claude Code sessions, in
parallel. Each session works in its own worktree and bills the API key of
the player. A stage is a brief in misc/stages/<stage>.md. The rules of all
stages are in misc/stages/common.md.

The stages that need no answer now:
- e3_native_names: the Names setting of E3;
- e4_officer_panel: the life and the Wikipedia button of E4;
- bio_check: a second source for each life of E4.

The stages e8_maps, e8_factions and e7_story need no answer either. The
player answered Q-E2 on 2026-10-29, and e6_church has its brief. E5 gets
a brief after the art arrives and the player answers Q-E3.

### The order of the stages
misc/stages/plan.tsv gives the order. A stage waits for another stage only
for a real need: a result that it uses, or a file that both stages edit.
All other stages run in parallel.

```mermaid
flowchart LR
    subgraph now["Ready now"]
        bio[bio_check<br/>data and docs]
        e3[e3_native_names<br/>text module, fonts, Settings]
        e4[e4_officer_panel<br/>text module, platform link]
        e3 --> e4
    end
    subgraph wait["Waits for answers"]
        q3{{Q-E3 licence}} --> art[e5 art<br/>external, Drive folder] --> e5[e5_arms<br/>flags, textures]
        q1{{Q-E2 answered}} --> e6[e6_church<br/>sees, caliphs, sim rules, engine_api]
        e8[e8_maps<br/>west and east packs] --> e6
        e8 --> e8f[e8_factions<br/>new factions of the east]
        e8f --> e7[e7_story<br/>story pages, events]
        e7 --> e6
    end
    e4 --> e5
    bio --> e7
    e4 --> check[check<br/>all tests, e2e, web build]
    bio --> check
    e5 --> check
    e7 --> check
```

The reasons for each arrow:
- e3_native_names before e4_officer_panel: both edit the text module and
  misc/make_scenario.py.
- e4_officer_panel before e5_arms: both add a function to the platform
  contract in code/hexdom.h.
- e7_story before e6_church: E6 waits for Q-E2. Thus e7_story leaves out
  the sees, and e6_church adds see_holder to the rows of e7_story.
- e8_maps before e6_church and e8_factions: the sees and the factions
  need the cities of the two maps.
- e8_factions before e7_story: both edit officers.tsv of the east pack.
  The events need the owners of Acre and Antioch.
- bio_check before e7_story: the pages of the rulers use the checked
  facts.
- e6_church edits code/sim/ and engine_api. It can run in parallel with
  e3_native_names and e4_officer_panel.

To run the stages:
1. Set the API key in your own terminal, not in a chat. The key goes into
   the user environment and stays out of the repository. The tool reads
   CLAUDE_API_KEY or ANTHROPIC_API_KEY:

       [Environment]::SetEnvironmentVariable("ANTHROPIC_API_KEY", "<key>", "User")

2. Print the waves of the plan. This step starts nothing:

       python -X utf8 misc\run_stages.py --plan --dry-run

3. Start the plan from the root of the repository. The tool stays open
   until the last stage ends:

       python -X utf8 misc\run_stages.py --plan

4. Look at the progress in another terminal:

       python -X utf8 misc\run_stages.py --status

To start one stage without the plan, give its name:

    python -X utf8 misc\run_stages.py bio_check

The tool makes the worktree ..\hexdom-<stage> on the branch
stage/<stage>. The session writes its output to build\stage.jsonl of that
worktree. The option --budget sets the largest cost of one session in
US dollars. The default is 20. The option --model sets the model. The
default is claude-sonnet-5-5. For bio_check, claude-haiku-5-5 costs less.

The session can edit files and run commands in its worktree. It cannot
force a push, reset a branch, delete a folder tree or use the stash. Each
session pushes to origin master after its tests pass. Two sessions can
edit the same file, for example the text module. Then the second session
merges the work of the first in its rebase.

## The open questions
- Q-E1. The five authorities. Dr. Casagranda names the Pentarchy: Rome,
  Alexandria, Antioch, Constantinople and Jerusalem. Is this the group of
  the player? Another choice is a group of the West: the pope and four
  great archbishops. The answer of 2026-10-28: the Pentarchy.
- Q-E2. The Muslim factions. Does the caliph of Baghdad act as a sixth
  authority for the Ayyubids and the Seljuks? Closed on 2026-10-29: the
  player chose two caliphs, Baghdad and Marrakesh (E6, "The two
  caliphs").
- Q-E3. The licence of the arms. Can the game use CC BY-SA images? Each
  one then needs credits, and a changed image keeps CC BY-SA.
- Q-E4. The scripts. Must the UI show Arabic, Greek and Cyrillic names in
  their own script? This needs new fonts and a shaper for Arabic.
- Q-E5. The story during the game. Must the story mode continue with
  dated events, or only give the background before the first turn? The
  answer of 2026-10-28: the story continues during the game. Each event
  starts from preconditions, not on a fixed date.
- Q-E6. The Three Kingdoms keys. Must the Three Kingdoms game also move
  to English keys? This needs a table of keys for approximately 1000
  officers of the original saves.
- Q-E7. Antioch is not a city of the map. Its see can stand in Jerusalem,
  in Konya, or in a new city. Another choice is a see without a city, held
  by no faction. The answer of 2026-10-28: a new city, with the story of
  the Mamluk siege of 1268.
- Q-E8. The place of Antioch. A save holds 42 cities. Can Antioch take
  the place of Barqa? The other choice is W3 of docs/MAPS.md, a pack with
  43 cities. Closed on 2026-10-28: the player chose two maps (E8). The
  east map holds Antioch, and Barqa stays.
