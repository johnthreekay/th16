#include <string.h>

#include "AnmManager.h"
#include "BulletManager.h"
#include "GameErrorContext.h"
#include "GameThread.h"
#include "UpdateFunc.h"
#include "ZunMath.h"

// GLOBAL: TH16 0x4a6dac
BulletManager *g_BulletManager;

// FUNCTION: TH16 0x411880
BulletManager::BulletManager()
{
    memset(this, 0, sizeof(BulletManager));
    g_BulletManager = this;
}

// FUNCTION: TH16 0x411940
Bullet::Bullet()
{
}

// FUNCTION: TH16 0x4119b0
Bullet::~Bullet()
{
}

// Puts every bullet on the free list and empties the tick list. Inlined into
// both callers in the original; our build needs the push.
__forceinline void BulletManager::reset_lists()
{
    ZunList<Bullet> *head = &freelist_head;
    head->entry = NULL;
    head->next = NULL;
    head->prev = NULL;
    head->unk_c = NULL;
    for (i32 i = 0; i < BULLET_COUNT; i++)
    {
        Bullet *b = &bullets[i];
        b->freelist_node.entry = NULL;
        b->freelist_node.next = NULL;
        b->freelist_node.prev = NULL;
        b->freelist_node.unk_c = NULL;
        b->tick_list_node.next = NULL;
        b->tick_list_node.entry = b;
        b->tick_list_node.prev = NULL;
        b->tick_list_node.unk_c = NULL;
        b->index = i;
        head->insert_after(&b->freelist_node);
    }
    tick_list_head.entry = NULL;
    tick_list_head.next = NULL;
    tick_list_head.prev = NULL;
    tick_list_head.unk_c = NULL;
}

// TODO: esi/edi get pushed after the early return, not at entry, and the
// loop stores b->freelist_node.entry through b, not the loop pointer.
// FUNCTION: TH16 0x411a30
i32 BulletManager::initialize()
{
    bullet_anm = AnmManager::preload_anm(7, "bullet.anm");
    if (bullet_anm == NULL)
    {
        // "Enemy bullet data not found. The data is corrupt."
        g_GameErrorContext.log("\x93G\x92" "e\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    next_free = bullets;
    bullets[BULLET_COUNT].state = BULLET_STATE_SENTINEL;

    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x1c);
    on_tick = f;

    f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x25);
    on_draw = f;

    reset_lists();
    return 0;
}

// TODO: the original has an unused 4-byte frame, keeps mgr in ebx, and
// stores the first loop field through the loop pointer.
// FUNCTION: TH16 0x411b70
void BulletManager::destroy_all()
{
    BulletManager *mgr = g_BulletManager;
    g_AnmManager->disable_vms_from_anm_file(mgr->bullet_anm);
    memset(mgr->bullets, 0, sizeof(mgr->bullets));
    mgr->next_free = mgr->bullets;
    mgr->bullets[BULLET_COUNT].state = BULLET_STATE_SENTINEL;
    memset(mgr->anm_ids, 0, BULLET_COUNT * sizeof(AnmId));
    mgr->reset_lists();
    mgr->ecl_unknown_560.x = 0.0f;
    mgr->ecl_unknown_560.y = 0.0f;
    mgr->unk_cancel_counter = 0;
    mgr->bullet_count_canceled_by_bombs = 0;
}

// FUNCTION: TH16 0x411ca0
BulletManager::~BulletManager()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    g_AnmManager->disable_vms_from_anm_file(bullet_anm);
    g_BulletManager = NULL;
}

// FUNCTION: TH16 0x411dd0
BulletManager *BulletManager::create()
{
    BulletManager *mgr = new BulletManager();
    if (mgr->initialize() != 0)
    {
        delete mgr;
        return NULL;
    }
    return mgr;
}

// FUNCTION: TH16 0x412c50
i32 __fastcall BulletManager::on_tick_callback(BulletManager *self)
{
    if (g_GameThread != NULL && (g_GameThread->flags | g_GameThread->flags >> 2) & 1)
    {
        return 1;
    }
    return self->on_tick_body();
}

// TODO: the original wraps a plain call in push ecx/pop ecx; ours tail-calls.
// FUNCTION: TH16 0x412c80
i32 __fastcall BulletManager::on_draw_callback(BulletManager *self)
{
    if (g_GameThread != NULL && g_GameThread->flags & GAME_THREAD_FLAG_4)
    {
        return 1;
    }
    return self->on_draw_body();
}

// FUNCTION: TH16 0x412a60
i32 BulletManager::on_draw_body()
{
    for (i32 i = 0; i < BULLET_LAYER_COUNT; i++)
    {
        for (Bullet *b = layer_heads[i]; b != NULL; b = b->next_in_layer)
        {
            // A byte read, which the compiler does not merge with the later
            // updates of flags_lo. ZUN's flags may well be bitfields.
            if (*(u8 *)&b->vm1.flags_lo & ANM_VM_VISIBLE)
            {
                b->vm1.pos = b->pos;
                if (b->vm1.flags_hi & ANM_VM_AUTO_ROTATE)
                {
                    b->vm1.rotation.z = normalize_angle(b->angle + ZUN_PI / 2);
                    b->vm1.flags_lo |= ANM_VM_ROTATION_CHANGED;
                }
                if (b->flags & BULLET_FLAG_SCALED)
                {
                    b->vm1.flags_lo |= ANM_VM_SCALE_CHANGED;
                    b->vm1.scale_2.x = b->scale;
                    b->vm1.scale_2.y = b->scale;
                }
                g_AnmManager->draw_vm(&b->vm1);
            }
            b->vm0.entity_pos = b->pos;
            if (b->vm0.flags_hi & ANM_VM_AUTO_ROTATE)
            {
                b->vm0.rotation.z = normalize_angle(b->angle + ZUN_PI / 2);
                b->vm0.flags_lo |= ANM_VM_ROTATION_CHANGED;
            }
            if (b->flags & BULLET_FLAG_SCALED)
            {
                b->vm0.flags_lo |= ANM_VM_SCALE_CHANGED;
                b->vm0.scale_2.x = b->scale;
                b->vm0.scale_2.y = b->scale;
            }
            g_AnmManager->draw_vm(&b->vm0);
        }
    }
    return 1;
}

// TODO: the original aligns the stack (and esp, -8) and keeps 1.0f in xmm2
// across the loop; the inlined Timer::increment differs a little too.
// FUNCTION: TH16 0x412860
i32 BulletManager::on_tick_body()
{
    Bullet *b;
    b = iter_first();
    bullet_count = 0;
    for (i32 i = BULLET_LAYER_COUNT - 1; i >= 0; i--)
    {
        layer_heads[i] = NULL;
    }
    for (i32 i = BULLET_LAYER_COUNT - 1; i >= 0; i--)
    {
        layer_tails[i] = NULL;
    }
    for (; b != NULL; b = iter_advance())
    {
        if (g_GameThread == NULL || !(g_GameThread->flags & GAME_THREAD_FLAG_400))
        {
            if (b->flags & BULLET_FLAG_100 && ((b->state == BULLET_STATE_2 && b->timer_144c.current >= 8) || b->state == BULLET_STATE_1))
            {
                b->sub_4124b0(1);
            }
            else if (b->on_tick() != 0)
            {
                continue;
            }
        }
        if (!(b->flags & BULLET_FLAG_NO_DRAW))
        {
            if (layer_heads[b->layer] != NULL)
            {
                layer_tails[b->layer]->next_in_layer = b;
            }
            else
            {
                layer_heads[b->layer] = b;
            }
            layer_tails[b->layer] = b;
            b->next_in_layer = NULL;
        }
        bullet_count++;
        b->timer_144c++;
    }
    return 1;
}
