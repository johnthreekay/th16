#pragma once

#include "types.h"

// Cursor state for a list menu, with a stack for submenus. Layout from
// ExpHP's th-re-data (zMenuHelper).
struct MenuHelper
{
    i32 next_selection;
    i32 current_selection;
    i32 num_choices;
    i32 stack_selection[0x10];
    i32 stack_num_choices[0x10];
    i32 stack_depth;
    // Choices the cursor skips over.
    i32 disabled[0x10];
    // Nonzero: moving past either end wraps around.
    i32 wraps;
    i32 num_disabled;

    MenuHelper()
    {
        stack_depth = 0;
        next_selection = 0;
        num_disabled = 0;
        wraps = 1;
        num_choices = 999;
    }

    i32 move_cursor(i32 delta);
    // 0x418720. Puts the cursor on a choice, clamped to the menu and moved
    // past disabled choices.
    i32 set_cursor(i32 selection);
    // 0x402de0. Enters a submenu, saving the cursor.
    void push();
    // 0x402e20. Back to the parent menu's cursor.
    void pop();
};
