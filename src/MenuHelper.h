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
    u8 unk_90[0xd0 - 0x90];
    i32 unk_d0;
    i32 unk_d4;

    MenuHelper()
    {
        stack_depth = 0;
        next_selection = 0;
        unk_d4 = 0;
        unk_d0 = 1;
        num_choices = 999;
    }
};
