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
Status: in progress (2026-10-20). Done and pushed (master 077fa0c):
- the merge of feat/default-portraits; portraits on the unit plates, in
  the bubble, in the officer view and in every officer picker;
- the proposal of the most suitable officers (ui_propose_officers);
- the command dialogs of the game mode: centred over a dark map, with
  portrait slots, the city panel hidden behind them;
- the game screens centred in every frame;
- one step for the city names at a game start.
- the city panel of the game mode with the fields of the original
  (ui_draw_city_game_fields in code/ui/ui_city.cpp): stock, 治安, 設施數,
  耐久, 士兵, 氣力, the weapons, the income, 市價, 現役, 俘虜, 在野, then
  the base command menu. The debug layout keeps its combo and slot table.
Next steps, in this order:
1. (Optional) The panel at the left or the right edge (Q-045). Today it
   opens next to the city through ui_context_place.
2. The officer selection window 選擇執行武將 (ui/city-commands.md
   "Officer selection lists"): tabs, columns, check boxes, sort, 一併 and
   一併解除. The 執行武將 button of each command dialog opens it.
3. The 開發 grid of facilities with the built counts, and the placement
   mode: the player clicks the slot on the map (ui/city-commands.md
   "Placement mode"). The engine has a query for the free slots.
4. Tutorial 32, the demo "demo/m19_city_scene", the Status block.

## M20 The turn of the others
Status: planned. The decision of the player (DECISIONS.md, 2026-10-20):
the show of the AI round is off by default and needs no click. The log
keeps the events. A setting turns the show on.

## M21 Information
Status: planned.

## M22 Fit and finish
Status: planned. The player confirmed the removals on 2026-10-20.
