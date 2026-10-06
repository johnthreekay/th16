#pragma once

#include "Timer.h"
#include "UpdateFunc.h"
#include "types.h"

struct GameThreadFlags
{
    u32 flag_0 : 1;
    // Lasers tick with the game speed forced to 0.
    u32 flag_1 : 1;
    // Lasers neither tick nor draw.
    u32 flag_2 : 1;
    u32 flag_3 : 7;
    u32 flag_10 : 1;
    u32 flag_11 : 21;
};

// Runs a game in progress. Layout from ExpHP's th-re-data (zGameThread).
struct GameThread
{
    u32 unk_0;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    Timer time_in_stage;
    u8 config[0x68];
    GameThreadFlags flags;
    u8 unk_8c[0xb4 - 0x8c];
};

extern GameThread *g_GameThread;
