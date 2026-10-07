#pragma once

#include "AnmManager.h"
#include "AnmVm.h"

#include <d3dx9math.h>
#include "UpdateFunc.h"
#include "decomp.h"
#include "types.h"

// How many effects EffectManager can track at once.
#define EFFECT_COUNT 0x400

// Rows of g_effect_table: the effects create_effect can start.
enum EffectId
{
    // The masked effect (anm_masked_effect_*): the main menu's transitions.
    EFFECT_MASKED = 0,
    // Children flying in to the VM (anm_gather_effect_*).
    EFFECT_GATHER = 1,
    // A jagged line growing from pos: blue and additive on layer 15, or gray
    // on layer 19 (used by a bomb).
    EFFECT_JAGGED_LINE_BLUE = 2,
    EFFECT_JAGGED_LINE_GRAY = 3,
};

// A row of the table EffectManager::create_effect reads (ExpHP:
// zTableAnm508Data): which script to start and the callbacks the VM gets.
struct EffectData
{
    // Index into the manager's ANM files (0: effect.anm, 1: bullet.anm).
    i16 anm_index;
    // Negative: no effect.
    i16 script;
    // Called with the new VM and the position passed to create_effect.
    i32(__fastcall *init)(AnmVm *vm, D3DXVECTOR3 *pos);
    i32 index_of_on_tick;
    i32 index_of_on_draw;
    i32 index_of_on_destroy;
    i32 index_of_on_interrupt;
    i32 index_of_on_copy;
    i32 index_of_on_serialize;
};

extern EffectData g_effect_table[4];

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

    // 0x418af0. Starts the effect (a g_effect_table row) at pos, or sets
    // up the given VM as one instead.
    AnmId create_effect(i32 effect, D3DXVECTOR3 *pos, AnmVm *vm);
    // 0x418ba0. create_effect for the UI list. Every caller (the main menu)
    // goes through g_EffectManager and passes (0, NULL, NULL).
    HARNESS_CALLED AnmId create_ui_effect(i32 effect, D3DXVECTOR3 *pos, AnmVm *vm);
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
