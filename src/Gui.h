#pragma once

#include "decomp.h"
#include "types.h"

// The HUD. Only the parts decompiled code calls so far.
struct Gui
{
    void update_lives(i32 lives, i32 fragments);
    void update_bombs(i32 bombs, i32 fragments);
    // Shows a HUD notice (2: full power, 4: extend). Its callers in the
    // original keep the stack 8-byte aligned for it (LTCG moved the
    // alignment out of the callee).
    void sub_42bcf0(i32 unk, i32 kind);
};

extern Gui *g_Gui;
