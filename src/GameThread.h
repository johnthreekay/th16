#pragma once

#include "UpdateFunc.h"
#include "types.h"

enum GameThreadFlags
{
    GAME_THREAD_FLAG_1 = 1 << 0,
    // Set while the game is paused: nothing in the stage ticks or draws.
    GAME_THREAD_FLAG_4 = 1 << 2,
    GAME_THREAD_FLAG_400 = 1 << 10,
};

// Runs a stage. Layout from ExpHP (zGameThread); mostly unknown here.
struct GameThread
{
    u8 unk_0[4];
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    u8 unk_c[0x88 - 0xc];
    u32 flags;

    void enable_update_funcs();
};

extern GameThread *g_GameThread;
