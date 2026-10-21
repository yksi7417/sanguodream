# Progress of M18 to M22

This file records the state of the milestones M18 to M22 of
docs/ROADMAP.md. The scheduled task san11-m18-m22 reads it at the start
of each run and writes it at the end. The next milestone to build is the
first section with a status other than "done". The milestones come from
the clone gap report (spec-src/spec/ui/clone-gap-report.md).

## M18 Feedback of a command
Status: done (2026-10-20, tutorial 31, demo "demo/m18_feedback").
Commits d1d08c5, f00d321, e064cb3 on master.

## M19 The city as a scene
Status: done (2026-10-21, tutorial 32, demo "demo/m19_city_scene").
Commits 077fa0c and before (portraits, proposal, framed dialogs, city
panel), then 11b9bab (officer selection, 開發 grid, placement mode) and
91171df (two fixes of the proposal, demo, tutorial) on master.
Tests: all unit and feature layers pass, ui 136/136 with e2e, the window
smoke run passes, and the 20 demos record.
Open points for the analysis side (also in the ROADMAP Status block):
- the columns of the first tab of 選擇執行武將 for each command;
- the other tabs of the list (所屬, 能力, 特技, 適性, 命令, 個人, 血緣);
- the rule when a row click goes past the officers of a command;
- the direction rule of a header sort (Q-046);
- the text of 效果 and 限制 in the 開發 dialog;
- the place of the city panel at the left or the right edge (Q-045).

## M20 The turn of the others
Status: done (2026-10-21, tutorial 33, demo "demo/m20_ai_round").
Commit 1be282c on master. The decision of the player (DECISIONS.md,
2026-10-20): the show of the AI round is off by default and needs no
click. The log keeps the events. A setting turns the show on.
Tests: all unit and feature layers pass, ui 139/139 with e2e, the window
smoke run passes, and the 21 demos record.
Open points for the analysis side (also in the ROADMAP Status block):
- the time of each step of an AI round (screen-flow.md, item 3);
- the path of a move of an AI unit (the ledger gives none);
- the effects of an attack and the cut-in 突擊!! (track G);
- a filter of the log for one faction or for the events near the player.

## M21 Information
Status: planned.

## M22 Fit and finish
Status: planned. The player confirmed the removals on 2026-10-20.
