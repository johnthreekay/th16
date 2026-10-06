#pragma once

// Minimal declarations of other units' classes and globals that unit 4's
// code touches. Only the fields used here are named; offsets follow
// ExpHP's th-re-data. To be replaced by the real headers when merged.

#include "../types.h"

// ExpHP: zGameThread.
struct GameThread
{
    u8 unk_0[0x88];
    u32 flag_0 : 1;
    u32 flag_1 : 1;
    u32 flag_2 : 1;
    u32 unk_flags_3 : 7;
    u32 flag_10 : 1;
    u32 unk_flags_11 : 21;
    u8 unk_8c[0xb4 - 0x8c];
};

// ExpHP: zPlayer.
struct Player
{
    u8 unk_0[0x1664c];
    u32 flags_1664c;
    u8 unk_16650[0x2c7cc - 0x16650];
    f32 damage_multiplier;
    u8 unk_2c7d0[0x2c828 - 0x2c7d0];
};

extern GameThread *g_GameThread;
extern Player *g_Player;
