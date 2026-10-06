#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "decomp.h"
#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far.
struct BulletManager
{
    u8 unk_0[0x1403b24];
    AnmLoaded *bullet_anm;

    // 0x416d20. Reaches the manager through its global, so LTCG drops the
    // unused this; the radius arrives in xmm2.
    HARNESS_CALLED void cancel_radius_as_bomb(D3DXVECTOR3 *pos, f32 radius, i32 mode);
};

extern BulletManager *g_BulletManager;
