#pragma once

#include "types.h"

// Cursor state shared by the game's menus (ExpHP: zMenuCommonThing).
struct MenuHelper
{
    i32 selection;
    i32 unk_4;
    i32 unk_8;
    u8 unk_c[0x8c - 0xc];
    i32 unk_8c;
    u8 unk_90[0xd0 - 0x90];
    i32 unk_d0;
    i32 unk_d4;

    MenuHelper()
    {
        unk_8c = 0;
        selection = 0;
        unk_d4 = 0;
        unk_d0 = 1;
        unk_8 = 999;
    }
};
