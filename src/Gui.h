#pragma once

#include "decomp.h"
#include "types.h"

// The HUD. Only the parts decompiled code uses so far; layout from ExpHP's
// th-re-data.
struct Gui
{
    u8 unk_0[0x1c8];
    // The dialogue being shown, if any.
    void *msg;

    void update_lives(i32 lives, i32 fragments);
    // 0x42c390
    void update_bombs(i32 bombs, i32 fragments);
    // Shows a HUD notice (2: full power, 4: extend). Its callers in the
    // original keep the stack 8-byte aligned for it (LTCG moved the
    // alignment out of the callee).
    void sub_42bcf0(i32 unk, i32 kind);
    // 0x42c600
    static void update_season_gauge();
};

extern Gui *g_Gui;
