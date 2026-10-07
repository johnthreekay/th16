#pragma once

#include "AnmManager.h"
#include "AnmVm.h"

#include <d3dx9math.h>
#include "UpdateFunc.h"
#include "decomp.h"
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

    // 0x418af0. Starts effect script `effect` at pos.
    AnmId create_effect(i32 effect, D3DXVECTOR3 *pos, i32 unk);
    // 0x40e6c0. Moves last_used_index on and returns the index before it,
    // clearing ids of finished effects on the way; -1 if all are taken.
    i32 next_index();
    // 0x40e730. create_effect, remembering the effect in anm_ids. Returns a
    // handle (index | 0x80000000), 0 if no slot is free. Reaches the
    // manager through g_EffectManager; every caller passes 0 for unused.
    HARNESS_CALLED i32 create_tracked(i32 effect, D3DXVECTOR3 *pos, i32 unused);

    // 0x41aa00. Remembers an existing VM like create_tracked does. Reaches
    // the manager through g_EffectManager.
    HARNESS_CALLED i32 track(AnmId id);

    // track as LTCG inlines it into some callers.
    i32 track_inline(AnmId id)
    {
        i32 index = next_index();
        if (index == -1)
        {
            return 0;
        }
        anm_ids[index] = id;
        return index | 0x80000000;
    }

    // create_tracked as LTCG inlines it into some callers.
    i32 create_tracked_inline(i32 effect, D3DXVECTOR3 *pos)
    {
        i32 index = next_index();
        if (index == -1)
        {
            return 0;
        }
        anm_ids[index] = create_effect(effect, pos, 0);
        return index | 0x80000000;
    }

    // The VM of a create_tracked handle (NULL for 0). Always inlined in the
    // original; ours would keep a copy out of line.
    __forceinline AnmVm *get_tracked_vm(i32 handle)
    {
        AnmId id;
        if (handle < 0)
        {
            id = anm_ids[(u16)handle];
        }
        else
        {
            id.id = 0;
        }
        return g_AnmManager->get_vm_with_id(id);
    }
};

extern EffectManager *g_EffectManager;

// Returns 0 once bullet.anm and effect.anm are loaded.
DECOMP_NOINLINE i32 preload_bullet_and_effect_anm();
