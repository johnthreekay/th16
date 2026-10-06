#pragma once

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
    u8 unk_0[0x88];
    u32 flags;
};

extern GameThread *g_GameThread;
