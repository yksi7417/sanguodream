# 31 Feedback: the text provider, the message log and the speech bubble

Files:
- code/engine/engine_report.cpp: event_report and the tone table;
- code/engine/engine.h: engine_event_report and engine_event_sign;
- code/text/text.h and text.cpp: text_provider, text_event_facts, the
  template provider and text_write_line;
- code/ui/ui_report.cpp: the log, the bubble and the values;
- code/ui/ui_feel.cpp: the turn start uses the report flag;
- data/placeholder/event_tones.tsv: the tone, the sign rule, the report
  flag and the log flag of each kind;
- data/placeholder/text/en.tsv and zh-TW.tsv: the "log." and "say."
  templates.

Tests: unit_text_provider_tests in tests/unit/text_tests.cpp,
tests/feature/report_tests.cpp, the ui tests
"command_feedback_opens_the_bubble_the_value_and_a_log_line" and
"a_second_text_provider_replaces_the_words", and the demo
"demo/m18_feedback".

## What it is for
Before M18 a command gave no feedback. The numbers changed, and nothing
else happened. The turn start showed every ledger event as an English
key and a number, for example 32 messages in one turn. This tutorial
builds milestone M18 (spec ui/clone-gap-report.md, sections 1, 4 and 7).

## The decision of the player
On 2026-10-20 the player decided where the words of a message come
from (spec DECISIONS.md):
- a text provider writes the words, and the provider is a plug-in;
- the first provider fills templates;
- a later provider can use a small language model;
- the tone, the personality of the speaker and the sign of the news
  matter more than the exact words of the original.

Thus the engine gives facts, and the front end writes words.

## The facts: event_report
engine_api.event_report takes the index of a ledger event. It gives:
- the kind and its stable key, for example "conscripted";
- the resource that the amount changes (gold, soldiers, public order
  and the others of sim_resource) and its key, so that a value names
  its field;
- the subject (a city or an officer), the faction, the amount;
- the date of the turn of the event;
- the speaker: the first officer of the command, else the officer of
  the event, the governor of its city or the ruler of its faction;
- the personality of the speaker;
- the tone, the sign (good, bad or neutral news), the report flag and
  the log flag, from event_tones.tsv.

The engine keeps the first officer of the last 256 commands, so that
each event of a command finds its speaker through its cause. A pack
without event_tones.tsv works: each kind is neutral, its sign follows
its amount, and nothing is a report.

## The words: text providers
text_provider is a name, a line function and a user pointer. The line
function takes the facts and a form:
- TEXT_LINE_LOG writes one line of the log in the third person;
- TEXT_LINE_SPEECH writes what the speaker says in the bubble.

The template provider looks up these strings in the locale:
1. a log line: "log.<key>", then "log.generic.quiet" for an event
   without an amount, then "log.generic". Every kind that reaches the
   log has its own line in both locales. The generic lines do not name
   the field, and the player asked what "+11" means. The test
   each_locale_has_a_log_line_for_each_event_kind keeps it so. The
   speaker slot of a line can hold a thing of the event. Examples are
   the weapon of the free weapons, the item, the post and the ability
   that changed. An oath or a marriage names the other officer there;
2. a speech: "say.<key>", then "say.tone.<tone>", then
   "say.sign.<sign>".

A template can hold {subject}, {speaker}, {faction}, {delta}, {amount}
and {value}. text_write_line calls the provider, and it falls back to
the templates when the provider gives no line. Thus a line is never
empty.

The templates use our own words. The spec forbids the text of the
original (DECISIONS.md, "What crosses the wall").

To plug in another provider, fill a text_provider and call
ui_set_text_provider. A provider without a line function gives back the
template provider.

## The UI
- The log (main-map.md, "Bottom-left panel") reads each new ledger
  event one time. It adds the date of each turn and one line for each
  event with the log flag. The colour of a line follows its sign. A
  hover shows the key and the tone. The filter shows all events or only
  those of the player. The log keeps the public events of the other
  factions (a destroyed unit, a retreat, a completed facility), as the
  player decided for M20. It leaves out their events that change a value
  (the numbers of their city commands, the losses of far units) and their
  bandits (G3-010, G3-017). After a load the log starts empty (G3-023).
  A line of a unit event names the unit
  by its commander (周泰隊：士兵 -232). The commander is the subject of
  the event, so the name stays after the unit is gone.
- The speech bubble (main-map.md, "Speech bubble") opens over the base
  after a command of the player. One of the executors of a city command
  speaks in the tone of the event. The command id picks him (G2-018,
  G3-014: the original picks at random). It does not take the mouse, and
  it closes after UI_BUBBLE_SECONDS.
- The values (main-map.md, "Floating value") rise over the base for each
  event of the command with a sign. Each value names its field first
  (資金收入 +2047), from the resource of the report. An arrow before the
  number points down for a loss and up for a gain (G2-018).
- The turn start shows the events with the report flag, then the lines
  of the adviser (G2-006, G2-007). The threat names an enemy city 10 days
  from an own city with more soldiers (assumed). The last line is
  請下達下一個指示. The
  first turn after the load says 可終於輪到我們了啊 in its place, and
  the camera stands at the capital. The camera moves to the city of each
  message. The window shows the speaker of each message and no counter.
  The messages that the player did not read close when the turn ends.

## What is assumed
The spec has no list of the kinds that the original logs or reports.
Thus the status of every row of event_tones.tsv is "assumed". The demo
gives one line for the main effect of a command, as the original does.
The turn start reports finished work.
