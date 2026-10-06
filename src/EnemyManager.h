#pragma once

#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far. Layout from ExpHP.
struct EnemyManager
{
    // The class has a vtable; its RTTI name is still to be checked.
    void *vtable;
    u8 unk_4[0x40 - 0x4];
    // Counts season releases in progress.
    i32 season_releases_active;
    i32 bomb_active;
    u8 unk_48[0x18c - 0x48];
    i32 enemy_count;
};

extern EnemyManager *g_EnemyManager;
