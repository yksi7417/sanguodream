# The research of E9: the candidates of the next generation

This document reports the first research of E9 (docs/EUROPE.md). It
explains docs/europe/E9-CANDIDATES.tsv and lists the decisions for the
review of the player.

## The result
The research of 2026-11-07 found 219 historical people for the three
packs of 1200. Each row of E9-CANDIDATES.tsv is one person.

| Pack | Candidates | Appeared and alive in 1220 | Appeared and alive in 1230 | Active officers in 1230 now |
|---|---|---|---|---|
| medieval_west | 132 | 51 | 80 | 19 |
| medieval_east | 121 | 40 | 70 | 15 |
| medieval_europe | 191 | 71 | 112 | 27 |

The count of a pack includes the people of the factions that the pack
shares with another map, for example the Papal States and Venice. If the
AI recruits most of the candidates, each pack passes the goal of E9. The
play run must measure this after the stage.

The roles of the 219 people:

| Role | People |
|---|---|
| ruler | 63 |
| general | 54 |
| churchman | 29 |
| statesman | 28 |
| scholar | 24 |
| regent | 13 |
| admiral | 5 |
| troubadour | 3 |

34 of the people are women. The setting 女武將 controls them.

## The method
1. Three research agents worked in parallel. One agent took the factions
   of the west, one the factions of the centre, and one the factions of
   the east and of the south.
2. Each agent got the factions, the cities of each map and the officers of
   the packs. It proposed only people that the packs do not have.
3. Each agent gave the years, the faction, the city of the first act, the
   family link, the role, two sources and a short life.
4. A merge script joined the three tables and checked each row:
   - each key and each name is unique, and no key is in a pack;
   - each faction is on each map of the column packs;
   - each city is a city of that map;
   - each parent is an officer of a pack;
   - each year of appearance is 1201 or later.
5. The three agents named Anthony of Padua. The table keeps the row of the
   west, with Portugal and Lisbon.
6. 24 rows of the west name a city that the prototype map does not have.
   The column city_europe gives the first city of the faction on that map
   instead, and year_notes says so.

## The check of the sources
CAUTION: The first research did not read the pages. The checks of
2026-11-09 below read Wikipedia and a second source for each row. Only
the rows with the source_status "two sources agree" meet the rule now.

The first research had these limits:
- The session did not get to en.wikipedia.org or britannica.com.
- The agents checked each person through the summaries of a web search.
- The searches of the session stopped at 200. Some facts came from the
  memory of the agent. The column year_notes says "from memory",
  "estimate" or "not checked on the web in this run".

## The check against Wikipedia
misc/e9_wiki_check.py reads the article of each row through the
MediaWiki API, on 2026-11-09. It writes the columns wiki_title,
wiki_birth, wiki_death and wiki_check into E9-CANDIDATES.tsv.

| wiki_check | Rows |
|---|---|
| ok | 135 |
| no birth category | 41 |
| no birth category; no death category | 14 |
| no birth category; death differs | 7 |
| birth differs | 7 |
| death differs | 7 |
| no death category | 3 |
| missing | 3 |
| birth differs; death differs | 2 |

- Three articles do not exist: albert_orlamunde, shahanshah_zakarian
  and ibn_al_alqami. The URL of premysl_moravia goes to the article of
  his father.
- A row with a difference keeps its years until two sources agree.
- The search for the second source runs next. Britannica refuses the
  container, so the search uses the national biographies and the other
  reference works.

## The check of the second sources
Two agents searched a second source for each row, on 2026-11-09. They
opened only these reference works:
- Treccani: the Enciclopedia Italiana, the Dizionario Biografico degli
  Italiani, the Federiciana and the Dizionario di Storia;
- encyclopedia.com: Columbia, the New Catholic Encyclopedia, Women in
  World History, the Encyclopedia of World Biography and other works;
- the Deutsche Biographie: NDB, ADB and the index of persons;
- the Catholic Encyclopedia and catholic-hierarchy.org.

These sources were not usable:
- Britannica and Iranica refuse the container.
- dbe.rah.es sends each page to historia-hispanica.rah.es, and the
  network does not allow that host. Most of the Iberian nobles thus
  have no second source.
- The copy of Setton, A History of the Crusades, at the University of
  Wisconsin asks for a check of the browser on another host.

The new columns of E9-CANDIDATES.tsv are second_url, second_work,
second_birth, second_death, second_agrees, second_note and
source_status. The value "partly" of second_agrees has two meanings.
The source gives only one year, and that year agrees. Or one year
differs by one. The column second_note tells the two cases apart.

Some rows use the entry of a parent or of a spouse that names the person
with years. The column second_work says so.

| source_status | West | Central | East | All |
|---|---|---|---|---|
| two sources agree | 32 | 29 | 7 | 68 |
| partly checked | 23 | 22 | 19 | 64 |
| one source | 13 | 12 | 30 | 55 |
| sources differ | 7 | 5 | 17 | 29 |
| no article | 1 | 0 | 2 | 3 |

The officers of each pack that appear by 1230 and live in 1230:

| Pack | Rows with two sources | With the partly checked rows | All rows |
|---|---|---|---|
| medieval_west | 27 | 55 | 80 |
| medieval_east | 10 | 33 | 70 |
| medieval_europe | 35 | 71 | 112 |

The east has the weakest sources. Most of the Ayyubid, Almohad, Rum and
Georgian people have only Wikipedia.

The sources differ on these years, for example:
- Ferdinand III: born 1198 in the New Catholic Encyclopedia, 1201 in the
  data.
- Kinga: born 1234 in Women in World History, 1224 in the data.
- Gonzalo de Berceo: died approximately 1252 in the Encyclopedia of World
  Biography, 1264 in the data.
- Marco Sanudo: born in the 1170s in the Dizionario Biografico, 1153 in
  the data.
- Michael II Komnenos Doukas: died 1271 in Treccani, 1268 in Wikipedia,
  1267 in the data.
- Philip of Novara: born approximately 1190 and died between 1261 and
  1264 in the Dizionario Biografico, 1200 and 1270 in the data.
- Fakhr al-Din ibn al-Shaykh: born approximately 1185 in Treccani, 1200
  in the data.

The column second_note gives each difference. The data does not change
until two sources agree.

## The columns of E9-CANDIDATES.tsv
| Column | Meaning |
|---|---|
| key | The English key of the person. It becomes the key of officers.tsv |
| group | The agent of the research: west, central or east |
| name_en, native_lang, name_native | The names, as in E3 |
| female | yes or no |
| birth, death, appear | The years. appear is the year of the first notable act |
| packs | The packs of the person: west, east or europe, with a bar between them |
| faction_key | The key of forces.tsv |
| city_west, city_east, city_europe | The city key of the first act on each map, or - |
| parent_key, relation | The officer of a pack who is the parent or the lord, and the link |
| role | ruler, general, admiral, statesman, regent, churchman, scholar or troubadour |
| wikipedia_url, second_source | The two sources |
| year_notes | The disagreements of the sources, the estimates and the stand-in cities |
| life | Three short sentences in the words of the research |
| confidence | high, medium or low |
| review | pending. The player writes accept or reject |
| problems | The findings of the merge script. All rows have - now |

## The decisions for the player
The agents found these points. Each needs an answer before the stage
e9_officers.

The faction of a person:
- Some people belong to one faction and act first in another place.
  Examples are Arthur of Brittany, Ferdinand of Flanders, Elizabeth of
  Hungary in Thuringia and Afonso III in Boulogne.
- Some people changed sides. Examples are Eustace the Monk, Savari de
  Mauléon, Olivier de Termes and Albert of Käfernburg.
- Alexios I of Trebizond is under the Byzantine Empire, but Trebizond was
  a separate state.
- Gurji Khatun is Georgian, but she is under the Sultanate of Rum.
- The Banu Ghaniya have no candidate. The research found no member of the
  next generation with an English article.

The cities:
- The maps do not have each city of the history. The agents used the
  nearest city of the faction. Examples are Athens for Epirus and Naxos,
  Fez for Tlemcen and Ceuta, and Milan for Verona.
- On the prototype map, the Ayyubid people of Syria and of the Jazira
  stand in Jerusalem, and the people of Nicaea stand in Constantinople.

The years:
- A few people were born before 1170 or after 1225. Examples are Marco
  Sanudo, Pelagius and Bolesław V.
- A few people appear after 1245. Examples are Ramón de Bonifaz in 1247
  and Michael VIII in 1246. Q-E12 is the question for them.
- year_notes gives each year that the sources give in different ways.

The family links:
- The column parent_key names only officers of the packs of 1200. The
  links between two new people are only in the life. An example is John
  Vatatzes and his son Theodore II.
- The column relation has a value "wife" that the plan did not list. The
  join row of Q-E11 needs a rule for a wife.
- Some parents are in another faction. Examples are Isabella of England,
  the daughter of John, in Sicily, and Kunigunde, the daughter of Philip
  of Swabia, in Bohemia.

## The next steps
1. Decide the rule for the review. A proposal: the stage takes the rows
   with "two sources agree", and the rows with "partly checked" where the
   second source agrees and gives only one year. A row with "sources differ" takes the
   year of the two sources that agree, or waits.
2. Find a second source for the 55 rows with one source. Most are of the
   east. A library catalogue or a printed work can give it. Allow
   historia-hispanica.rah.es for the Iberian nobles.
3. The player answers Q-E11 to Q-E13 and the decisions above, and writes
   accept or reject in the column review.
4. The stage e9_officers adds the accepted rows (misc/stages/e9_officers.md).
