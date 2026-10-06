#include <string.h>

#include "AnmManager.h"
#include "EffectManager.h"
#include "GameErrorContext.h"
#include "UpdateFunc.h"

// GLOBAL: TH16 0x4a6db8
EffectManager *g_EffectManager;

// FUNCTION: TH16 0x418790
i32 preload_bullet_and_effect_anm()
{
    if (AnmManager::preload_anm(8, "effect.anm") == NULL)
    {
        // "Effect data not found. The data is corrupt."
        g_GameErrorContext.log("\x83G\x83t\x83" "F\x83N\x83g\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    if (AnmManager::preload_anm(7, "bullet.anm") == NULL)
    {
        g_GameErrorContext.log("\x83G\x83t\x83" "F\x83N\x83g\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    return 0;
}

// TODO: the original keeps the UpdateFunc constructor's stores but still
// merges the two flag updates; volatile here keeps both apart.
// FUNCTION: TH16 0x4187d0
i32 EffectManager::initialize()
{
    bullet_anm = AnmManager::preload_anm(7, "bullet.anm");
    if (bullet_anm == NULL)
    {
        // "Screen layout data not found. The data is corrupt."
        g_GameErrorContext.log("\x89\xe6\x96\xca\x8d\\\x90\xac\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    effect_anm = AnmManager::preload_anm(8, "effect.anm");
    if (effect_anm == NULL)
    {
        g_GameErrorContext.log("\x89\xe6\x96\xca\x8d\\\x90\xac\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }

    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x1f);
    on_tick = f;

    // create_func, inlined here in the original.
    volatile UpdateFunc *vf = new UpdateFunc();
    vf->flags |= UPDATE_FUNC_HEAP_ALLOCATED;
    vf->function = (UpdateFuncCallback)on_draw_callback;
    vf->on_registration = NULL;
    vf->on_cleanup = NULL;
    f = (UpdateFunc *)vf;
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x26);
    on_draw = f;

    on_tick->flags |= UPDATE_FUNC_ACTIVE;
    on_draw->flags |= UPDATE_FUNC_ACTIVE;
    return 0;
}

// TODO: register allocation in the unregister part (this in ebx, one spill).
// FUNCTION: TH16 0x4188d0
EffectManager::~EffectManager()
{
    g_AnmManager->disable_vms_from_anm_file(effect_anm);
    g_AnmManager->disable_vms_from_anm_file(bullet_anm);
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    g_AnmManager->unload_anm(8);
    g_AnmManager->unload_anm(7);
    g_EffectManager = NULL;
}

EffectManager::EffectManager()
{
    memset(this, 0, sizeof(EffectManager));
    flags |= 2;
    g_EffectManager = this;
}

// FUNCTION: TH16 0x418a30
EffectManager *EffectManager::create()
{
    EffectManager *mgr = new EffectManager();
    if (mgr->initialize() != 0)
    {
        delete mgr;
        return NULL;
    }
    return mgr;
}

// FUNCTION: TH16 0x418ab0
i32 __fastcall EffectManager::on_tick_callback(EffectManager *self)
{
    AnmManager *anm = g_AnmManager;
    for (i32 i = 0; i < EFFECT_COUNT; i++)
    {
        if (anm->get_vm_with_id(self->anm_ids[i]) == NULL)
        {
            self->anm_ids[i].id = 0;
        }
    }
    return 1;
}

// FUNCTION: TH16 0x418ae0
i32 __fastcall EffectManager::on_draw_callback(EffectManager *self)
{
    return 1;
}

// FUNCTION: TH16 0x40e6c0
i32 EffectManager::next_index()
{
    for (i32 i = 0; i < EFFECT_COUNT; i++)
    {
        i32 index = last_used_index;
        last_used_index = (last_used_index + 1) % EFFECT_COUNT;
        AnmId &id = anm_ids[last_used_index];
        if (id.id == 0)
        {
            return index;
        }
        if (g_AnmManager->get_vm_with_id(id) != NULL)
        {
            return index;
        }
        id.id = 0;
    }
    return -1;
}

// FUNCTION: TH16 0x40e730
HARNESS_CALLED i32 EffectManager::create_tracked(i32 effect, D3DXVECTOR3 *pos, i32 unused)
{
    i32 index = next_index();
    if (index == -1)
    {
        return 0;
    }
    anm_ids[index] = create_effect(effect, pos, 0);
    return index | 0x80000000;
}
