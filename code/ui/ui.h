#ifndef UI_H
#define UI_H
/* Debug UI state. Lives inside game_state in the permanent block so it
   survives DLL reloads. bool rather than b32 because ImGui writes these. */

struct ui_state
{
    bool show_demo;
    bool show_sim;
    bool show_ledger;
    bool show_memory;
};

internal void
ui_init(ui_state *ui)
{
    ui->show_demo   = true;
    ui->show_sim    = true;
    ui->show_ledger = true;
    ui->show_memory = false;
}

#endif /* UI_H */
