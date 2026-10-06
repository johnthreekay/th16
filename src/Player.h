#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "Timer.h"
#include "decomp.h"
#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far. Layout from ExpHP.

struct PlayerInner
{
    D3DXVECTOR3 pos;
    u8 unk_c[0x16028 - 0xc];
    Timer iframes;
    u8 unk_1603c[0x16078 - 0x1603c];
    // Set every frame by the autumn release.
    f32 speed_multiplier;
    u8 unk_1607c[0x16090 - 0x1607c];

    // 0x4440e0
    void repopulate_options();
};

struct Player
{
    u8 unk_0[0xc];
    AnmLoaded *anm_file;
    AnmLoaded *subseason_anm_file;
    u8 unk_14[0x610 - 0x14];
    PlayerInner inner;
    u8 unk_166a0[0x2c7cc - 0x166a0];
    // Set every frame by the winter release.
    f32 damage_multiplier;
    u8 unk_2c7d0[0x2c828 - 0x2c7d0];

    // 0x4449b0. Returns the index of the new damage source plus one.
    HARNESS_CALLED i32 create_damage_source(D3DXVECTOR3 *pos, f32 radius, f32 unk, i32 unk_2, i32 damage);
};

extern Player *g_Player;
