#pragma once

#include "types.h"

// Runs a stage. Only what unit 6 needs so far; layout from ExpHP's
// th-re-data (zGameThread).
struct GameThread
{
    u8 unk_0[0x88];
    // 0x4: paused.
    u32 flags;
    u8 unk_8c[0xb4 - 0x8c];
};

extern GameThread *g_GameThread;
