#pragma once

#include "decomp.h"
#include "types.h"

// Owns the effect ANMs. Only what unit 5's code calls so far.
struct EffectManager
{
    u8 unk_0[0x2020];

    ~EffectManager();
    static EffectManager *create();
};

extern EffectManager *g_EffectManager;

// Returns 0 once bullet.anm and effect.anm are loaded.
i32 preload_bullet_and_effect_anm();
