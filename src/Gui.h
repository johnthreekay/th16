#pragma once

#include "types.h"

// The HUD. Only what unit 5's code calls so far.
struct Gui
{
    void update_bombs(i32 bombs, i32 fragments);
};

extern Gui *g_Gui;
