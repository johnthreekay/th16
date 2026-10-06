#pragma once

#include "AnmManager.h"
#include "AnmVm.h"
#include "UpdateFunc.h"
#include "types.h"

#define EFFECT_COUNT 0x400

// Fire-and-forget ANM effects (explosions, item sparkles, ...). Layout from
// ExpHP (zEffectManager).
struct EffectManager
{
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    AnmLoaded *effect_anm;
    AnmLoaded *bullet_anm;
    u8 unk_14[4];
    i32 last_used_index;
    AnmId anm_ids[EFFECT_COUNT];
    i32 snapshot_last_used_index;
    AnmId snapshot_anm_ids[EFFECT_COUNT];

    EffectManager();
    ~EffectManager();

    static EffectManager *create();
    i32 initialize();
    static i32 __fastcall on_tick_callback(EffectManager *self);
    static i32 __fastcall on_draw_callback(EffectManager *self);
};

extern EffectManager *g_EffectManager;

i32 preload_bullet_and_effect_anm();
