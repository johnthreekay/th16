#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "Timer.h"
#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far. Layout from ExpHP.

struct PlayerInner
{
    D3DXVECTOR3 pos;
    u8 unk_c[0x16028 - 0xc];
    Timer iframes;
    u8 unk_1603c[0x16090 - 0x1603c];

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
};

extern Player *g_Player;
