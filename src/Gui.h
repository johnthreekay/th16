#pragma once

#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far. Layout from ExpHP.
struct Gui
{
    u8 unk_0[0x1c8];
    // The dialogue being shown, if any.
    void *msg;

    // 0x42c390
    void update_bombs(i32 bombs, i32 fragments);
    // 0x42c600
    static void update_season_gauge();
};

extern Gui *g_Gui;
