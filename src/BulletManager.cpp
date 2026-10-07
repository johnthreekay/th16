#include <math.h>
#include <string.h>

#include "AnmManager.h"
#include "BulletManager.h"
#include "Bomb.h"
#include "Collision.h"
#include "Enemy.h"
#include "EnemyManager.h"
#include "Item.h"
#include "Laser.h"
#include "Player.h"
#include "Rng.h"
#include "SoundManager.h"
#include "Spellcard.h"
#include "GameErrorContext.h"
#include "GameThread.h"
#include "Gui.h"
#include "UpdateFunc.h"
#include "ZunAngle.h"
#include "ZunMath.h"

// GLOBAL: TH16 0x4a6dac
BulletManager *g_BulletManager;

// This file's copy of ZunMath.h's sincosmul, which TH16 keeps once per
// object file. A static of its own so that it can be annotated.
// FUNCTION: TH16 0x417510
static void __fastcall bullet_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

// A second copy, which only step_ex_08 calls.
// FUNCTION: TH16 0x4173a0
static void __fastcall bullet_sincosmul_2(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

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
    if (g_GameThread != NULL && (g_GameThread->flags.flag_0 | g_GameThread->flags.paused))
    {
        return 1;
    }
    return self->on_tick_body();
}

// TODO: the original wraps a plain call in push ecx/pop ecx; ours tail-calls. A harness
// standing in for thread_start's aligned create() call (create and initialize HARNESS_CALLED)
// matches this and lifts initialize to 96%, but on_tick_callback then pads its call too,
// because our on_tick_body does not realign itself like the original's.
// FUNCTION: TH16 0x412c80
i32 __fastcall BulletManager::on_draw_callback(BulletManager *self)
{
    if (g_GameThread != NULL && g_GameThread->flags.paused)
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
                    b->vm1.rotation.z = wrap_angle(b->angle + ZUN_PI / 2);
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
                b->vm0.rotation.z = wrap_angle(b->angle + ZUN_PI / 2);
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
// across the loop; the inlined ZunTimer::tick differs a little too.
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
        if (g_GameThread == NULL || !g_GameThread->flags.flag_10)
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
        b->timer_144c.tick();
    }
    return 1;
}

// FUNCTION: TH16 0x417140
int __fastcall bullet_map_sprite(AnmVm *vm, i32 sprite)
{
    Bullet *bullet = (Bullet *)vm->associated_game_entity;
    if (g_bullet_types[bullet->sprite].sprites[0][0] >= 0)
    {
        return g_bullet_types[bullet->sprite].sprites[bullet->color][sprite];
    }
    return sprite;
}

static_assert(offsetof(Bullet, cancel_script) == 0xc5c, "Bullet layout");
static_assert(offsetof(Bullet, timer_144c) == 0x144c, "Bullet layout");
static_assert(offsetof(BulletManager, anm_ids) == 0x13ffc8c, "BulletManager layout");
static_assert(offsetof(BulletManager, unk_cancel_counter) == 0x1403b14, "BulletManager layout");

// Bullet::cancel's body, which clear_all has inlined.
static __forceinline i32 cancel_bullet(Bullet *bullet, i32 mode)
{
    bullet->vm0.interrupt(1);
    bullet->vm0.run();
    if (bullet->vm1.flags_lo & ANM_VM_VISIBLE)
    {
        bullet->vm1.interrupt(1);
    }
    if (!(bullet->flags & BULLET_FLAG_NO_DRAW))
    {
        if (bullet->cancel_script >= 0)
        {
            AnmLoaded *anm = g_BulletManager->bullet_anm;
            g_BulletManager->anm_ids[bullet->index] = anm->create_vm(bullet->cancel_script, &bullet->pos, 0.0f, -1, 0);
        }
        g_SoundManager.play_sound_at_position(0x47, bullet->pos.x);
        gen_items_from_cancel(&bullet->pos, mode);
    }
    D3DXVECTOR3 delta = bullet->velocity * g_game_speed * 0.5f;
    bullet->pos += delta;
    bullet->state = 4;
    bullet->timer_144c.reset();
    return 0;
}

// FUNCTION: TH16 0x416840
i32 Bullet::cancel(i32 mode)
{
    return cancel_bullet(this, mode);
}

// TODO: the inlined cancel_bullet's velocity scaling and pos += delta differ in register
// allocation and scheduling (the out-of-line Bullet::cancel matches).
// FUNCTION: TH16 0x416f40
HARNESS_CALLED void BulletManager::clear_all(i32 unused)
{
    Bullet *bullet = g_BulletManager->bullets;
    for (i32 i = 0; i < BULLET_COUNT; i++, bullet++)
    {
        if (bullet->state != BULLET_STATE_FREE && bullet->state != 3)
        {
            cancel_bullet(bullet, 0);
        }
    }
}

// Whether a bullet's hitbox touches a circle.
static inline i32 bullet_in_circle(Bullet *bullet, D3DXVECTOR3 *pos, f32 radius)
{
    f32 r = bullet->hitbox_diameter * 0.5f + radius;
    f32 dx = bullet->pos.x - pos->x;
    f32 dy = bullet->pos.y - pos->y;
    return dy * dy + dx * dx <= r * r;
}

// TODO: only the iterator differs: the original does not thread the jump after the
// iterator's NULL entry (if/else, or iter_* defined out of line, do not change it).
// FUNCTION: TH16 0x416c20
HARNESS_CALLED i32 BulletManager::cancel_radius(D3DXVECTOR3 *pos, f32 radius, i32 mode)
{
    Bullet *bullet = g_BulletManager->iter_first();
    while (bullet != NULL)
    {
        if (bullet->state == BULLET_STATE_2 || bullet->state == BULLET_STATE_1)
        {
            if (bullet_in_circle(bullet, pos, radius))
            {
                bullet->cancel(mode);
            }
        }
        bullet = g_BulletManager->iter_advance();
    }
    return 0;
}

// TODO: as cancel_radius (only the iterator's jump threading differs).
// FUNCTION: TH16 0x416d20
HARNESS_CALLED i32 BulletManager::cancel_radius_as_bomb(D3DXVECTOR3 *pos, f32 radius, i32 mode)
{
    for (Bullet *bullet = g_BulletManager->iter_first(); bullet != NULL; bullet = g_BulletManager->iter_advance())
    {
        if ((bullet->state == BULLET_STATE_2 || bullet->state == BULLET_STATE_1) &&
            bullet->ex_invuln_remaining_frames == 0)
        {
            if (bullet_in_circle(bullet, pos, radius))
            {
                bullet->cancel(mode);
            }
        }
    }
    return 0;
}

// FUNCTION: TH16 0x416e20
HARNESS_CALLED i32 BulletManager::cancel_rectangle_as_bomb(D3DXVECTOR3 *pos, D3DXVECTOR3 *size, f32 angle, i32 mode)
{
    Bullet *bullet = bullets;
    for (i32 i = 0; i < BULLET_COUNT; i++, bullet++)
    {
        if ((bullet->state == BULLET_STATE_2 || bullet->state == BULLET_STATE_1) &&
            bullet->ex_invuln_remaining_frames == 0)
        {
            f32 radius = bullet->scale * bullet->hitbox_diameter;
            if (collision_test_circle_rect(pos->x, pos->y, size->x, size->y, angle, bullet->pos.x, bullet->pos.y,
                                           radius) &&
                collision_test_circle_rect(0.0f, 224.0f, 384.0f, 448.0f, 0.0f, bullet->pos.x, bullet->pos.y, radius))
            {
                bullet->cancel(mode);
            }
        }
    }
    return 0;
}

// FUNCTION: TH16 0x416a00
HARNESS_CALLED void gen_items_from_cancel(D3DXVECTOR3 *pos, i32 mode)
{
    if (mode == 0)
    {
        return;
    }
    if (pos->x + 32.0f <= -192.0f || pos->x - 32.0f >= 192.0f || pos->y + 32.0f <= 0.0f || pos->y - 32.0f >= 448.0f)
    {
        return;
    }
    BulletManager *mgr = g_BulletManager;
    mgr->unk_cancel_counter++;
    if (mode == 1 || mode == 3)
    {
        return;
    }
    if (mode == 2)
    {
        if (mgr->cancel_counter_multiple_of(5) && !(g_Spellcard->flags & 1))
        {
            g_ItemManager->spawn_item(1, pos, 0, -ZUN_PI / 2.0f, 2.2f, 0, 0);
        }
        if (!(g_Spellcard->flags & 1))
        {
            g_ItemManager->spawn_item(10, pos, 0, g_replay_safe_rng.randf_neg_to(ZUN_PI / 180.0f * 10.0f) - ZUN_PI / 2.0f,
                                      2.2f, 0, 0);
        }
    }
    else if (mode == 5)
    {
        if (mgr->bomb_cancel_count_multiple_of(3))
        {
            g_ItemManager->spawn_item(16, pos, 0, g_replay_safe_rng.randf_neg_to(ZUN_PI / 180.0f * 10.0f) - ZUN_PI / 2.0f,
                                      2.2f, 0, 1);
        }
        g_BulletManager->bullet_count_canceled_by_bombs++;
    }
    else if (mode == 4)
    {
        g_ItemManager->spawn_item(16, pos, 0, g_replay_safe_rng.randf_neg_to(ZUN_PI / 180.0f * 10.0f) - ZUN_PI / 2.0f, 2.2f,
                                  0, 1);
        if (g_SubseasonBomb->in_use == 1)
        {
            g_ItemManager->spawn_item(g_SubseasonBomb->season_level + 8, pos, 0,
                                      g_replay_safe_rng.randf_neg_to(ZUN_PI / 180.0f * 10.0f) - ZUN_PI / 2.0f, 2.2f, 0, 0);
        }
    }
}

// Cancel scripts of bullet.anm for types of cancel kind 1, by color.
// GLOBAL: TH16 0x490ec0
extern const i32 g_bullet_cancel_scripts[8] = {4, 8, 12, 16, 20, 24, 28, 34};

// TODO: register allocation of the angle math differs (the original adds into angle's register and wraps the angle in xmm0), and case 5 does not share case 3's tail.
// FUNCTION: TH16 0x412cb0
i32 BulletManager::shoot_one(EnemyBulletShooter *props, i32 i, i32 layer, f32 angle_to_player)
{
    Bullet *bullet = (Bullet *)freelist_head.next;
    if (bullet == NULL)
    {
        return 1;
    }
    bullet->freelist_node.unlink_inline();
    g_BulletManager->tick_list_head.insert_after(&bullet->tick_list_node);

    f32 angle = 0.0f;
    f32 speed;
    if (props->layers > 1)
    {
        speed = props->spd1 - (props->spd1 - props->spd2) * (f32)layer / (f32)(props->layers - 1);
    }
    else
    {
        speed = props->spd1;
    }
    f32 x;
    switch ((u16)props->aim_type)
    {
    case 0:
    case 1:
        if (props->count & 1)
        {
            angle += (f32)((i + 1) / 2) * props->ang_bullet_dist;
        }
        else
        {
            angle += (f32)(i / 2) * props->ang_bullet_dist + props->ang_bullet_dist * 0.5f;
        }
        if (i & 1)
        {
            angle *= -1.0f;
        }
        if ((u16)props->aim_type == 0)
        {
            angle += angle_to_player;
        }
        angle += props->ang_aim;
        break;
    case 2:
        angle += angle_to_player;
    case 3:
        angle += (f32)i * ZUN_2PI / (f32)props->count;
        angle += (f32)layer * props->ang_bullet_dist + props->ang_aim;
        break;
    case 4:
        angle += angle_to_player;
    case 5:
        angle += ZUN_PI / (f32)props->count;
        angle += (f32)i * ZUN_2PI / (f32)props->count;
        angle += (f32)layer * props->ang_bullet_dist + props->ang_aim;
        break;
    case 6:
        angle = props->ang_aim + g_replay_safe_rng.randf_neg_to(props->ang_bullet_dist);
        break;
    case 7:
        speed = g_replay_safe_rng.randf_0_to(props->spd2) + props->spd1;
        angle += (f32)i * ZUN_2PI / (f32)props->count;
        angle += (f32)layer * props->ang_bullet_dist + props->ang_aim;
        break;
    case 8:
        angle = props->ang_aim + g_replay_safe_rng.randf_neg_to(props->ang_bullet_dist);
        speed = g_replay_safe_rng.randf_0_to(props->spd2) + props->spd1;
        break;
    case 9:
    case 10:
        x = (f32)i * ZUN_2PI / (f32)props->count;
        if (props->layers & 1)
        {
            angle = (f32)((layer + 1) / 2) * props->ang_bullet_dist + x;
            if (props->layers > 1)
            {
                speed = (props->spd2 - props->spd1) * (f32)((layer + 1) & 0xfffe) / (f32)(props->layers - 1) +
                        props->spd1;
            }
        }
        else
        {
            angle = (f32)(layer / 2) * props->ang_bullet_dist + props->ang_bullet_dist * 0.5f + x;
            if (props->layers > 1)
            {
                speed = (props->spd2 - props->spd1) * (f32)(layer & 0xfffe) / (f32)(props->layers - 1) + props->spd1;
            }
        }
        if (layer & 1)
        {
            angle *= -1.0f;
        }
        if ((u16)props->aim_type == 9)
        {
            angle += angle_to_player;
        }
        angle += props->ang_aim;
        break;
    case 11:
        x = (f32)i * ZUN_2PI / (f32)props->count;
        angle += props->ang_aim + x;
        speed *= 1.0f - (f32)fabs(sinf(x)) * props->spd2;
        break;
    case 12:
        x = (f32)i * ZUN_2PI / (f32)props->count + ZUN_PI / (f32)props->count;
        angle += props->ang_aim + x;
        speed *= 1.0f - (f32)fabs(sinf(x)) * props->spd2;
        break;
    }
    bullet->speed = speed;
    bullet->angle = wrap_angle(wrap_angle(angle + 0.0f));
    bullet->pos = props->pos;
    if (props->distance != 0.0f)
    {
        D3DXVECTOR3 offset;
        bullet_sincosmul(&offset, bullet->angle, props->distance);
        bullet->pos.x += offset.x;
        bullet->pos.y += offset.y;
    }
    bullet->pos.z = 0.1f;
    bullet->flags |= 1;
    bullet->state = 1;
    bullet->timer_144c.reset();
    bullet->timer_1460.reset();
    bullet->ex_invuln_remaining_frames = 0;
    bullet->scale = 1.0f;
    bullet->scale_i.end_time = 0;
    if (et_protect_range > 0.0f)
    {
        if (et_protect_range > (bullet->pos.x - g_Player->inner.pos.x) * (bullet->pos.x - g_Player->inner.pos.x) +
                                   (bullet->pos.y - g_Player->inner.pos.y) * (bullet->pos.y - g_Player->inner.pos.y))
        {
            bullet->sub_412670();
            return -1;
        }
    }
    bullet_sincosmul(&bullet->velocity, angle, speed);
    bullet->active_ex_flags = props->sfx_flags;
    bullet->color = props->color;
    bullet->sprite = props->type;
    bullet->unk_c7c = 0;
    bullet->flags = (bullet->flags & ~0xc) | 2;
    bullet->unk_1448 = 60;
    bullet->timer_1420.reset();
    bullet->timer_1434.reset();
    AnmVm *vm = &bullet->vm0;
    vm->wipe();
    bullet->vm0.index_of_sprite_mapping_func = 1;
    bullet->vm0.associated_game_entity = bullet;
    bullet_anm->set_vm_script(&bullet->vm0, g_bullet_types[props->type].script);
    bullet->flags |= 0x10;
    bullet->vm0.flags_hi = (bullet->vm0.flags_hi & ~0x80000) | ANM_VM_LAYER_SET;
    bullet->vm1.wipe();
    bullet->vm1.flags_lo &= ~1;
    if (g_bullet_types[props->type].unk_110 != 0)
    {
        bullet->vm1.flags_lo |= 1;
        g_BulletManager->bullet_anm->set_vm_script(&bullet->vm1, g_bullet_types[props->type].unk_110);
        bullet->vm1.flags_hi = (bullet->vm1.flags_hi & ~0x80000) | ANM_VM_LAYER_SET;
    }
    switch (g_bullet_types[props->type].unk_10c)
    {
    case 0:
        bullet->cancel_script = props->color * 2 + 4;
        break;
    case 1:
        bullet->cancel_script = g_bullet_cancel_scripts[props->color];
        break;
    case 2:
        bullet->cancel_script = -1;
        bullet->flags |= 0x10;
        break;
    case 3:
        bullet->cancel_script = 0x10;
        break;
    case 4:
        bullet->cancel_script = 6;
        break;
    case 6:
        bullet->cancel_script = g_bullet_types[bullet->sprite].sprites[props->color][3];
        bullet->flags |= 0x10;
        break;
    case 7:
        bullet->cancel_script = 0x104;
        bullet->flags |= 0x10;
        break;
    case 8:
        bullet->cancel_script = 0x107;
        bullet->flags |= 0x10;
        break;
    case 9:
        bullet->cancel_script = 0x10a;
        bullet->flags |= 0x10;
        break;
    case 10:
        bullet->cancel_script = 0x113;
        bullet->flags |= 0x10;
        break;
    }
    bullet->layer = g_bullet_types[props->type].unk_108;
    bullet->bounce_sound = props->shot_transform_sfx;
    bullet->unk_c58 = 5;
    bullet->hitbox_diameter = bullet->hitbox_height = g_bullet_types[props->type].hitbox_radius;
    bullet->unk_c6c = props->sfx_flags;
    bullet->active_ex_flags = 0;
    bullet->unk_c64 = 0;
    bullet->unk_c60 = props->start_transform;
    memcpy(bullet->et_ex, props->ex, sizeof(bullet->et_ex));
    if (props->ex[props->start_transform].type == 2)
    {
        if ((i16)props->ex[bullet->unk_c60].a != 1)
        {
            bullet->vm0.interrupt_out_of_line((i16)props->ex[bullet->unk_c60].a + 7);
        }
        bullet->state = 2;
        bullet->pos -= bullet->velocity * 4.0f;
        bullet->unk_c60++;
    }
    else
    {
        vm->interrupt(2);
    }
    bullet->run_ex();
    vm->run();
    if (bullet->vm1.flags_lo & 1)
    {
        bullet->vm1.run();
    }
    return 0;
}

// TODO: about half the code differs: ours addresses et_ex by index instead of through an ex pointer kept in esi, hoists constants, and speculatively devirtualizes the inlined lasers' initialize calls.
// Starts the et_ex transforms from unk_c60 on, until one has to wait: an
// empty slot, a slot-0 transform while others still run, or a transform of
// a kind already running. Angle arguments of -999990 keep the bullet's
// angle and 999990 or more aim at the player.
// FUNCTION: TH16 0x413860
void Bullet::run_ex()
{
    while (unk_c60 < 0x12)
    {
        i32 index = unk_c60;
        BulletEx *ex = et_ex;
        ex += index;
        u32 type = ex->type;
        if (type == 0)
        {
            return;
        }
        if (ex->slot == 0 && (active_ex_flags & ~0x100))
        {
            return;
        }
        if (type & active_ex_flags)
        {
            return;
        }
        switch (type)
        {
        case 2:
            vm0.interrupt_out_of_line((i16)ex->a + 7);
            state = 2;
            pos -= velocity * 4.0f;
            break;
        case 1:
            active_ex_flags |= 1;
            ex_state[0].timer.set_value(0);
            ex_state[0].floats[7] = 0.0f;
            break;
        case 4:
        {
            active_ex_flags |= 4;
            ex_state[1].floats[0] = ex->r;
            ZunAngle angle = ex->s <= -999990.0f
                                 ? angle_ref()
                                 : ZunAngle(ex->s >= 999990.0f ? g_Player->angle_to_player(&pos) + ex->m : ex->s);
            ex_state[1].floats[1] = angle.value;
            ex_state[1].timer.set_value(0);
            ex_state[1].ints[0] = ex->a;
            bullet_sincosmul((Float3 *)&ex_state[1].floats[5], ex_state[1].floats[1], ex_state[1].floats[0]);
        play_sound:
            if (unk_c60 != 0 && bounce_sound >= 0)
            {
                g_SoundManager.play_sound_centered(bounce_sound, 0);
            }
            break;
        }
        case 8:
            active_ex_flags |= 8;
            ex_state[2].floats[0] = ex->r;
            ex_state[2].floats[1] = ex->s;
            ex_state[2].timer.set_value(0);
            ex_state[2].ints[0] = ex->a;
            goto play_sound;
        case 0x10:
        {
            active_ex_flags |= 0x10;
            ex_state[3].floats[0] = ex->s > -999990.0f ? ex->s : speed;
            f32 r = ex->r;
            ZunAngle angle;
            if (r <= -999990.0f)
            {
                angle = angle_ref();
            }
            else
            {
                angle = ZunAngle(r >= 9999990.0f  ? ex_state[12].floats[1]
                                 : r >= 999990.0f ? g_Player->angle_to_player(&pos) + ex->m
                                                  : r);
            }
            switch (ex->c)
            {
            case 0:
            case 1:
            case 4:
                ex_state[3].floats[1] = angle.value;
                break;
            case 2:
                ex_state[3].floats[1] =
                    normalize_angle(g_Player->angle_to_player((D3DXVECTOR3 *)&ex_state[12].floats[2]) + angle.value);
                break;
            case 3:
                ex_state[3].floats[1] = normalize_angle(ex_state[12].floats[1] + angle.value);
                break;
            case 5:
            case 6:
                ex_state[3].floats[1] = g_replay_safe_rng.randf_neg_1_to_1() * ex->r;
                break;
            case 7:
                ex_state[3].floats[1] =
                    (ex->r <= -999990.0f ? angle_ref()
                                         : ZunAngle(ex->r >= 990.0f ? g_Player->angle_to_player(&pos) : ex->r))
                        .value;
                ex_state[3].floats[0] = speed + g_replay_safe_rng.randf_neg_1_to_1() * ex->s;
                break;
            }
            ex_state[3].timer.set_value(0);
            ex_state[3].ints[0] = ex->a;
            ex_state[3].ints[1] = ex->b;
            ex_state[3].ints[2] = 0;
            ex_state[3].ints[3] = ex->c;
            ex_state[3].ints[4] = ex->d;
            break;
        }
        case 0x40:
            active_ex_flags |= 0x40;
            ex_state[4].floats[0] = ex->r;
            if (ex->b & 0x20)
            {
                ex_state[4].floats[2] = ex->s;
                ex_state[4].floats[3] = ex->m;
            }
            else
            {
                ex_state[4].floats[2] = 384.0f;
                ex_state[4].floats[3] = 448.0f;
            }
            ex_state[4].ints[1] = ex->a;
            ex_state[4].ints[0] = 0;
            ex_state[4].ints[3] = ex->b;
            break;
        case 0x80:
            ex_invuln_remaining_frames = ex->a;
            break;
        case 0x100:
            active_ex_flags |= 0x100;
            ex_state[11].timer.set(ex->a);
            ex_state[11].ints[0] = ex->b;
            unk_c60++;
            continue;
        case 0x800:
            g_SoundManager.play_sound_at_position(ex->a, pos.x);
            unk_c60++;
            continue;
        case 0x400:
            if (ex->a == 1)
            {
                cancel_script = -1;
            }
            cancel(0);
            break;
        case 0x200:
            sprite = ex->a;
            color = ex->b & 0x7fff;
            hitbox_diameter = hitbox_height = g_bullet_types[ex->a].hitbox_radius;
            layer = g_bullet_types[sprite].unk_108;
            vm0.wipe();
            vm0.index_of_sprite_mapping_func = 1;
            vm0.associated_game_entity = this;
            g_BulletManager->bullet_anm->set_vm_script(&vm0, g_bullet_types[ex->a].script);
            flags |= 0x10;
            vm0.flags_hi = (vm0.flags_hi & ~0x80000) | ANM_VM_LAYER_SET;
            vm1.wipe();
            vm1.flags_lo &= ~1;
            if (g_bullet_types[ex->a].unk_110 != 0)
            {
                vm1.flags_lo |= 1;
                g_BulletManager->bullet_anm->set_vm_script(&vm1, g_bullet_types[ex->a].unk_110);
                vm1.flags_hi = (vm1.flags_hi & ~0x80000) | ANM_VM_LAYER_SET;
            }
            switch (g_bullet_types[sprite].unk_10c)
            {
            case 0:
                cancel_script = color * 2 + 4;
                break;
            case 1:
                cancel_script = g_bullet_cancel_scripts[color];
                break;
            case 2:
                cancel_script = -1;
                flags |= 0x10;
                break;
            case 3:
                cancel_script = 0x10;
                break;
            case 4:
                cancel_script = 6;
                break;
            case 5:
                cancel_script = 0xc;
                break;
            case 6:
                cancel_script = g_bullet_types[sprite].sprites[color][3];
                flags |= 0x10;
                break;
            case 7:
                cancel_script = 0x104;
                flags |= 0x10;
                break;
            case 8:
                cancel_script = 0x107;
                flags |= 0x10;
                break;
            case 9:
                cancel_script = 0x10a;
                flags |= 0x10;
                break;
            case 10:
                cancel_script = 0x113;
                flags |= 0x10;
                break;
            }
            if (ex->b & 0x8000)
            {
                anm_vm_interrupt_2(&vm0);
            }
            break;
        case 0x1000:
            active_ex_flags |= 0x1000;
            ex_state[6].ints[1] = ex->a;
            ex_state[6].ints[0] = 0;
            ex_state[6].ints[2] = ex->b;
            break;
        case 0x8000:
            unk_c4c = ex->a;
            unk_c60 = index + 1;
            continue;
        case 0x2000:
        {
            EnemyBulletShooter props;
            props.pos = pos;
            *(u16 *)&props.aim_type = ex->a;
            props.start_transform = ex->b;
            props.count = ex->c;
            props.layers = ex->d;
            if (ex->r <= -999990.0f)
            {
                props.ang_aim = angle;
            }
            else
            {
                props.ang_aim = wrap_angle(ex->r >= 999990.0f ? g_Player->angle_to_player(&pos) : ex->r);
            }
            props.ang_bullet_dist = ex->s;
            props.spd1 = ex->m <= -999990.0f ? speed : ex->m;
            props.spd2 = ex->n;
            unk_c60 = index + 1;
            props.type = ex[1].a;
            i32 cancel_after = ex[1].c;
            props.color = ex[1].b;
            props.sfx_flags = 0;
            memcpy(props.ex, et_ex, sizeof(props.ex));
            g_BulletManager->shoot_bullets(&props);
            unk_c60++;
            if (cancel_after != 0)
            {
                cancel(0);
            }
            continue;
        }
        case 0x10000:
            if (ex->b <= 0)
            {
                unk_c60 = ex->a;
                continue;
            }
            if (unk_c64 == 0)
            {
                unk_c64 = ex->b;
                unk_c60 = ex->a;
                continue;
            }
            if (unk_c64 == 1)
            {
                unk_c64 = 0;
                break;
            }
            unk_c64--;
            unk_c60 = ex->a;
            continue;
        case 0x80000:
            active_ex_flags |= 0x80000;
            bullet_sincosmul((Float3 *)&ex_state[9].floats[5], ex->r, ex->s);
            ex_state[9].floats[7] = 0.0f;
            ex_state[9].floats[1] = ex->r;
            ex_state[9].floats[0] = ex->s;
            ex_state[9].ints[0] = ex->a;
            ex_state[9].timer.set_value(0);
            break;
        case 0x40000:
            if (ex->r >= 990.0f)
            {
                f32 offset = ex->r - 999.0f;
                angle_ref() = add_normalize_angle(g_Player->angle_to_player(&pos), offset);
            }
            else if (ex->r >= -990.0f)
            {
                angle_ref() = ex->r;
            }
            if (ex->s >= -990.0f)
            {
                speed = ex->s;
            }
            bullet_sincosmul(&velocity, angle, speed);
            unk_c60++;
            continue;
        case 0x20000:
        {
            active_ex_flags |= 0x20000;
            D3DXVECTOR3 *target = (D3DXVECTOR3 *)&ex_state[8].floats[5];
            target->x = ex->r;
            target->y = ex->s;
            if (ex->b & 0x100)
            {
                target->x += pos.x;
                target->y += pos.y;
                target->z += pos.z;
            }
            ex_state[8].floats[0] = speed;
            target->z = 0.0f;
            ex_state[8].ints[0] = ex->a;
            ex_state[8].ints[1] = (u8)ex->b;
            ex_state[8].timer.reset_inline();
            ex_move_i.initial = pos;
            ex_move_i.goal = *target;
            ex_move_i.bezier_1 = g_zero_vec;
            ex_move_i.bezier_2 = g_zero_vec;
            ex_move_i.end_time = ex->a;
            ex_move_i.method = (u8)ex->b;
            ex_move_i.reset_timer();
            break;
        }
        case 0x100000:
            if (ex->a == 2)
            {
                ((AnmVmFlagsLoBits *)&vm0.flags_lo)->blend_mode = 2;
            }
            else if (ex->a == 1)
            {
                ((AnmVmFlagsLoBits *)&vm0.flags_lo)->blend_mode = 1;
            }
            else
            {
                ((AnmVmFlagsLoBits *)&vm0.flags_lo)->blend_mode = 0;
            }
            unk_c60++;
            continue;
        case 0x400000:
            active_ex_flags |= 0x400000;
            scale_i.initial = ex->r;
            scale_i.goal = ex->s;
            scale_i.bezier_1 = 0.0f;
            scale_i.bezier_2 = 0.0f;
            scale_i.end_time = ex->a;
            scale_i.method = ex->b;
            scale_i.reset();
            flags |= BULLET_FLAG_SCALED;
            unk_c60++;
            continue;
        case 0x200000:
            active_ex_flags |= 0x200000;
            ex_state[10].floats[0] = (ex->r - speed) / (f32)ex->a;
            if (ex->s <= -999990.0f)
            {
                ex_state[10].floats[1] = angle;
            }
            else
            {
                ex_state[10].floats[1] =
                    wrap_angle(ex->s >= 999990.0f ? g_Player->angle_to_player(&pos) + ex->m : ex->s);
            }
            ex_state[10].timer.reset_inline();
            ex_state[10].ints[0] = ex->a;
            bullet_sincosmul((Float3 *)&ex_state[10].floats[5], ex_state[10].floats[1], ex_state[10].floats[0]);
            goto play_sound;
        case 0x800000:
            *(D3DXVECTOR3 *)&ex_state[12].floats[2] = pos;
            ex_state[12].floats[1] = angle;
            ex_state[12].floats[0] = speed;
            unk_c60++;
            continue;
        case 0x4000000:
            if (ex->a <= 0)
            {
                unk_c60 = index + 1;
                continue;
            }
            active_ex_flags |= 0x4000000;
            ex_state[13].timer.set_value(ex->a);
            break;
        case 0x2000000:
            layer = ex->a;
            unk_c60 = index + 1;
            continue;
        case 0x1000000:
        {
            EnemyCreateParams params;
            memset(&params, 0, sizeof(params));
            params.pos = pos;
            params.ecl_int_vars[0] = ex->a;
            params.ecl_int_vars[1] = ex->b;
            params.ecl_int_vars[2] = ex->c;
            params.ecl_int_vars[3] = ex->d;
            params.life = 10000;
            params.score_reward = 0;
            params.item_drop = 0;
            memcpy(params.ecl_float_vars, &ex->r, 4 * sizeof(f32));
            g_EnemyManager->allocate_new_enemy(ex->string, &params, 0);
            break;
        }
        case 0x8000000:
            if (ex->a == 0)
            {
                LaserLineInner params;
                memcpy(params.ex, et_ex, sizeof(params.ex));
                params.start_pos = pos;
                params.bullet_type = ex->b;
                params.bullet_color = ex->c;
                u32 cancel_after = ex->d;
                params.ang_aim =
                    (ex->r <= -999990.0f ? angle_ref()
                                         : ZunAngle(ex->r >= 999990.0f ? g_Player->angle_to_player(&pos) : ex->r))
                        .value;
                params.speed = ex->s <= -999990.0f ? speed : ex->s;
                params.flags |= 1;
                params.laser_new_arg_1 = ex->m;
                params.laser_new_arg_2 = ex->n;
                unk_c60++;
                params.laser_new_arg_3 = ex[1].r;
                params.laser_new_arg_4 = ex[1].s;
                params.distance = ex[1].m;
                params.shot_sfx = ex[1].a;
                params.shot_transform_sfx = ex[1].b;
                params.unk_30 = ex[1].c;
                LaserManager *mgr = g_LaserManager;
                if (mgr->list_length < 0x200)
                {
                    mgr->last_id++;
                    if (mgr->last_id < 0x10000)
                    {
                        mgr->last_id = 0x10000;
                    }
                    LaserDataInf *laser = new LaserLineInf();
                    laser->id = mgr->last_id;
                    mgr->append(laser);
                    laser->initialize(&params);
                }
                unk_c60++;
                if (cancel_after != 0)
                {
                    cancel(0);
                }
                continue;
            }
            else if (ex->a == 1)
            {
                LaserInfiniteInner params;
                memcpy(params.ex, et_ex, sizeof(params.ex));
                params.start_pos = pos;
                u32 d = ex->d;
                params.flags = (d & 0xfd) | 2;
                params.type = ex->b;
                params.color = ex->c;
                *(i32 *)params.unk_50 = (u8)(d >> 8);
                params.ang_aim =
                    (ex->r <= -999990.0f ? angle_ref()
                                         : ZunAngle(ex->r >= 999990.0f ? g_Player->angle_to_player(&pos) : ex->r))
                        .value;
                params.speed = ex->s <= -999990.0f ? speed : ex->s;
                params.laser_new_arg_1 = ex->m;
                params.laser_new_arg_2 = ex->n;
                unk_c60++;
                params.unk_30 = ex[1].a;
                params.unk_34 = ex[1].b;
                params.unk_38 = ex[1].c;
                params.unk_3c = ex[1].d;
                params.laser_new_arg_4 = ex[1].r;
                params.distance = ex[1].s;
                params.shot_sfx = 0x12;
                params.shot_transform_sfx = -1;
                LaserManager *mgr = g_LaserManager;
                if (mgr->list_length < 0x200)
                {
                    mgr->last_id++;
                    if (mgr->last_id < 0x10000)
                    {
                        mgr->last_id = 0x10000;
                    }
                    LaserDataInf *laser = new LaserInfiniteInf();
                    laser->id = mgr->last_id;
                    mgr->append(laser);
                    laser->initialize(&params);
                }
                unk_c60++;
                if (d & 0x10000)
                {
                    cancel(0);
                }
                continue;
            }
            continue;
        case 0x80000000:
            if (ex->a <= 0)
            {
                unk_c60 = index + 1;
                continue;
            }
            active_ex_flags |= 0x80000000;
            ex_state[5].timer.set_value(ex->a);
            break;
        case 0x20000000:
        {
            f32 size = ex->r;
            if (0.0f > size)
            {
                size = g_bullet_types[sprite].hitbox_radius;
            }
            hitbox_diameter = size;
            hitbox_height = size;
            break;
        }
        }
        unk_c60++;
    }
}

// FUNCTION: TH16 0x414da0
HARNESS_CALLED i32 BulletManager::shoot_bullets(EnemyBulletShooter *props)
{
    f32 dy = g_Player->inner.pos.y - props->pos.y;
    f32 dx = g_Player->inner.pos.x - props->pos.x;
    f32 angle;
    if (dy == 0.0f && dx == 0.0f)
    {
        angle = ZUN_PI / 2;
    }
    else
    {
        angle = atan2f(dy, dx);
    }
    for (i32 layer = 0; layer < props->layers; layer++)
    {
        for (i32 i = 0; i < props->count; i++)
        {
            i32 result = shoot_one(props, i, layer, angle);
            if (result != 0 && result == 1)
            {
                goto done;
            }
        }
    }
done:
    if (props->sfx_flags & 0x20)
    {
        g_SoundManager.play_sound_at_position(props->shot_sfx, props->pos.x);
    }
    return 0;
}

static_assert(offsetof(Bullet, ex_state) == 0xfa0, "Bullet layout");

// TODO: in the inlined timer tick the original adds the speed to current_f in xmm0 and
// jumps to shared stores (ours adds current_f to the speed in xmm1; tick_mixed gets closer).
// FUNCTION: TH16 0x414ec0
i32 Bullet::step_ex_00()
{
    if (ex_state[0].timer.current <= 16)
    {
        bullet_sincosmul(&velocity, angle, 5.0f - ex_state[0].timer.current_f * 5.0f / 16.0f + speed);
        ex_state[0].timer.tick_mixed();
        return 0;
    }
    active_ex_flags ^= 1;
    return 1;
}

// FUNCTION: TH16 0x415790
i32 Bullet::bounce_left()
{
    f32 width = g_BulletManager->ecl_unknown_560.x;
    if (width <= 0.0f)
    {
        width = ex_state[4].floats[2];
    }
    if (width * -0.5f > pos[0])
    {
        if (!(ex_state[4].ints[3] & 0x10))
        {
            angle = wrap_angle(-angle - ZUN_PI);
            angle = wrap_angle(wrap_angle(angle + 0.0f));
            pos[0] = -width - pos[0];
        }
        return 1;
    }
    return 0;
}

// FUNCTION: TH16 0x4158d0
i32 Bullet::bounce_right()
{
    f32 width = g_BulletManager->ecl_unknown_560.x;
    if (width <= 0.0f)
    {
        width = ex_state[4].floats[2];
    }
    if (pos[0] >= width * 0.5f)
    {
        if (!(ex_state[4].ints[3] & 0x10))
        {
            angle = wrap_angle(-angle - ZUN_PI);
            angle = wrap_angle(wrap_angle(angle + 0.0f));
            pos[0] = width - pos[0];
        }
        return 1;
    }
    return 0;
}

// FUNCTION: TH16 0x415a10
i32 Bullet::bounce_top()
{
    f32 height = g_BulletManager->ecl_unknown_560.y;
    if (height <= 0.0f)
    {
        height = ex_state[4].floats[3];
    }
    if (pos[1] < 224.0f - height * 0.5f)
    {
        if (!(ex_state[4].ints[3] & 0x10))
        {
            angle = wrap_angle(-angle);
            pos[1] = 448.0f - height - pos[1];
        }
        return 1;
    }
    return 0;
}

// FUNCTION: TH16 0x415ae0
i32 Bullet::bounce_bottom()
{
    f32 height = g_BulletManager->ecl_unknown_560.y;
    if (height <= 0.0f)
    {
        height = ex_state[4].floats[3];
    }
    if (pos[1] >= height * 0.5f + 224.0f)
    {
        if (!(ex_state[4].ints[3] & 0x10))
        {
            angle = wrap_angle(-angle);
            pos[1] = height + 448.0f - pos[1];
        }
        return 1;
    }
    return 0;
}

// Whether a point (grown by radius) is outside the rectangle of the given
// size centered on the playfield's middle.
static inline i32 outside_bounce_rect(D3DXVECTOR3 *p, f32 radius, f32 width, f32 height)
{
    f32 x = p->x;
    if (x + radius <= width * -0.5f || x - radius >= width * 0.5f)
    {
        return 1;
    }
    f32 y = p->y;
    if (y + radius <= 224.0f - height * 0.5f || y - radius >= height * 0.5f + 224.0f)
    {
        return 1;
    }
    return 0;
}

// FUNCTION: TH16 0x415bb0
i32 Bullet::step_ex_06()
{
    BulletManager *mgr = g_BulletManager;
    if ((mgr->ecl_unknown_560.x <= 0.0f &&
         outside_bounce_rect(&pos, 0.0f, ex_state[4].floats[2], ex_state[4].floats[3])) ||
        (mgr->ecl_unknown_560.x > 0.0f &&
         outside_bounce_rect(&pos, 0.0f, mgr->ecl_unknown_560.x, mgr->ecl_unknown_560.y)))
    {
        i32 bounced = 0;
        if (ex_state[4].ints[3] & 1)
        {
            if (bounce_top())
            {
                bounced = 1;
            }
        }
        if (ex_state[4].ints[3] & 2)
        {
            if (bounce_bottom())
            {
                bounced = 1;
            }
        }
        if (ex_state[4].ints[3] & 8)
        {
            if (bounce_right())
            {
                bounced = 1;
            }
        }
        if (ex_state[4].ints[3] & 4)
        {
            if (bounce_left())
            {
                bounced = 1;
            }
        }
        if (ex_state[4].floats[0] > -990.0f)
        {
            speed = ex_state[4].floats[0];
        }
        bullet_sincosmul(&velocity, angle, speed);
        if (bounced)
        {
            ex_state[4].ints[0]++;
            if (bounce_sound >= 0)
            {
                g_SoundManager.play_sound_centered(bounce_sound, 0);
            }
        }
        if (ex_state[4].ints[0] >= ex_state[4].ints[1])
        {
            active_ex_flags &= ~0x40;
            return 1;
        }
    }
    return 0;
}

// FUNCTION: TH16 0x4161f0
i32 Bullet::step_ex_19()
{
    if (ex_state[9].timer.current >= ex_state[9].ints[0])
    {
        active_ex_flags &= 0xfffffff6;
        return 1;
    }
    pos += *(D3DXVECTOR3 *)&ex_state[9].floats[5] * g_game_speed;
    ex_state[9].timer.reset();
    return 0;
}

// A ZunAngle operator as LTCG inlines it into step_ex_03: the store goes
// through the angle's pointer, so the angle is read again afterwards.
static void add_angle_twice(ZunAngle *a, f32 delta)
{
    a->value = wrap_angle(wrap_angle(a->value + delta));
}

// FUNCTION: TH16 0x4153e0
i32 Bullet::step_ex_03()
{
    if (ex_state[2].timer.current >= ex_state[2].ints[0])
    {
        active_ex_flags &= ~8;
        return 1;
    }
    add_angle_twice(&angle_ref(), ex_state[2].floats[1] * g_game_speed);
    speed += ex_state[2].floats[0] * g_game_speed;
    bullet_sincosmul(&velocity, angle, speed);
    ex_state[2].timer.tick_mixed();
    return 0;
}

// TODO: in the inlined timer tick (tick_mixed) the original loads current_f into xmm0
// and adds the speed (ours adds current_f into the speed's xmm1).
// FUNCTION: TH16 0x415570
i32 Bullet::step_ex_04()
{
    f32 new_speed;
    if (ex_state[3].timer.current >= ex_state[3].ints[0])
    {
        if (bounce_sound >= 0)
        {
            g_SoundManager.play_sound_centered(bounce_sound, 0);
        }
        ex_state[3].ints[2]++;
        switch (ex_state[3].ints[3])
        {
        case 0:
        case 5:
            angle_ref() += ex_state[3].floats[1];
            break;
        case 1:
        case 6:
            angle_ref() = g_Player->angle_to_player(&pos) + ex_state[3].floats[1];
            break;
        case 2:
        case 3:
        case 4:
            angle_ref() = ex_state[3].floats[1];
            break;
        }
        new_speed = ex_state[3].floats[0];
        speed = new_speed;
        ex_state[3].timer.reset();
        if (ex_state[3].ints[2] >= ex_state[3].ints[1])
        {
            bullet_sincosmul(&velocity, angle, new_speed);
            active_ex_flags &= ~0x10;
            return 1;
        }
    }
    else
    {
        new_speed = speed - ex_state[3].timer.current_f * speed / ex_state[3].ints[0];
    }
    bullet_sincosmul(&velocity, angle, new_speed);
    ex_state[3].timer.tick_mixed();
    return 0;
}

// FUNCTION: TH16 0x414fb0
i32 Bullet::step_ex_02()
{
    if (ex_state[1].timer.current >= ex_state[1].ints[0])
    {
        active_ex_flags &= ~4;
        return 1;
    }
    speed += ex_state[1].floats[0] * g_game_speed;
    velocity += *(D3DXVECTOR3 *)&ex_state[1].floats[5] * g_game_speed;
    if (fabsf(velocity.x) > 0.0001f || fabsf(velocity.y) > 0.0001f)
    {
        angle = wrap_angle(atan2(velocity.y, velocity.x));
        speed = D3DXVec2Length((D3DXVECTOR2 *)&velocity);
    }
    ex_state[1].timer.tick();
    return 0;
}

// FUNCTION: TH16 0x4151e0
i32 Bullet::step_ex_21()
{
    if (ex_state[10].timer.current >= ex_state[10].ints[0])
    {
        active_ex_flags &= ~0x200000;
        return 1;
    }
    speed += ex_state[10].floats[0] * g_game_speed;
    velocity += *(D3DXVECTOR3 *)&ex_state[10].floats[5] * g_game_speed;
    if (fabsf(velocity.x) > 0.0001f || fabsf(velocity.y) > 0.0001f)
    {
        angle = wrap_angle(atan2(velocity.y, velocity.x));
    }
    ex_state[10].timer.tick();
    return 0;
}

// The sprite a VM shows. Not inline: the original looks it up again for
// every use.
static AnmLoadedSprite *vm_sprite(AnmVm *vm)
{
    return &g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
}

// Whether something of the given size at x is entirely outside [lo, hi].
static i32 outside_range(f32 x, f32 size, f32 lo, f32 hi)
{
    f32 half = size * 0.5f;
    return x + half <= lo || x - half >= hi;
}

// TODO: the original keeps all five constants in registers from the start
// and adds the half size to the position (ours the other way round).
// FUNCTION: TH16 0x415d80
i32 Bullet::step_ex_12()
{
    if (outside_range(pos.x, vm_sprite(&vm0)->sprite_width, -192.0f, 192.0f) ||
        outside_range(pos.y, vm_sprite(&vm0)->sprite_height, 0.0f, 448.0f))
    {
        i32 sides = ex_state[6].ints[2];
        if ((sides & 1) && pos.y < 0.0f)
        {
            pos.y = vm_sprite(&vm0)->sprite_height + 448.0f + pos.y;
        }
        else if ((sides & 2) && pos.y > 448.0f)
        {
            pos.y = pos.y - (vm_sprite(&vm0)->sprite_height + 448.0f);
        }
        else if ((sides & 4) && pos.x < -192.0f)
        {
            pos.x = vm_sprite(&vm0)->sprite_width + 384.0f + pos.x;
        }
        else if ((sides & 8) && pos.x > 192.0f)
        {
            pos.x = pos.x - (vm_sprite(&vm0)->sprite_width + 384.0f);
        }
        else
        {
            return 0;
        }
        ex_state[6].ints[0]++;
        if (bounce_sound >= 0)
        {
            g_SoundManager.play_sound_centered(bounce_sound, 0);
        }
        if (ex_state[6].ints[0] >= ex_state[6].ints[1])
        {
            active_ex_flags ^= 0x1000;
            return 1;
        }
    }
    return 0;
}

// TODO: the original stores pos.z after loading the angle, and the inlined
// timer tick keeps the frame in xmm0 (ours xmm1).
// FUNCTION: TH16 0x415f90
i32 Bullet::step_ex_17()
{
    if (ex_state[8].timer.current >= ex_state[8].ints[0])
    {
        pos = *(D3DXVECTOR3 *)&ex_state[8].floats[5];
        active_ex_flags &= ~0x20000;
        speed = ex_state[8].floats[0];
        bullet_sincosmul(&velocity, angle, speed);
        velocity.z = 0.0f;
        return 1;
    }
    if (ex_state[8].timer.current == 0)
    {
        ex_move_i.initial = pos;
    }
    D3DXVECTOR3 prev = pos;
    velocity = ex_move_i.step() - prev;
    if (fabsf(velocity.x) > 0.0001f || fabsf(velocity.y) > 0.0001f)
    {
        angle = wrap_angle(atan2(velocity.y, velocity.x));
    }
    velocity.z = 0.0f;
    ex_state[8].timer.tick_mixed();
    return 0;
}

// TODO: the original saves ebx and edi in the prologue, keeps
// cancel_script in ecx and the manager in eax, and puts goal 4 bytes lower.
// FUNCTION: TH16 0x4124b0
i32 Bullet::sub_4124b0(i32 graze_only)
{
    vm0.flags_lo &= ~0x60000;
    vm0.pos = g_zero_vec;
    if ((flags & 2) && hitbox_diameter > 0.0f)
    {
        Float3 *hitbox = (Float3 *)&hitbox_diameter;
        Float3 *p = &pos;
        i32 result;
        if (!(flags & BULLET_FLAG_SCALED))
        {
            if (!(flags & 0x10))
            {
                result = g_Player->check_hit_rect(p, hitbox, graze_only);
            }
            else
            {
                result = g_Player->check_hit_circle(p, hitbox_diameter, graze_only);
            }
        }
        else if (!(flags & 0x10))
        {
            D3DXVECTOR3 size;
            size.x = (*hitbox)[0] * scale;
            size.y = (*hitbox)[1] * scale;
            result = g_Player->check_hit_rect(p, &size, graze_only);
        }
        else
        {
            result = g_Player->check_hit_circle(p, scale * hitbox_diameter, graze_only);
        }
        if (result == 1)
        {
            if (ex_invuln_remaining_frames == 0)
            {
                state = 3;
                vm0.interrupt_out_of_line(result);
                if (vm1.flags_lo & 1)
                {
                    vm1.interrupt_out_of_line(result);
                }
                if (cancel_script >= 0)
                {
                    BulletManager *mgr = g_BulletManager;
                    AnmVm *vm = mgr->bullet_anm->create_vm(cancel_script, p, 0.0f, -1, 0).find_or_clear();
                    D3DXVECTOR3 goal = g_game_speed * velocity * 10.0f;
                    vm->set_pos_time(30, 6, &g_zero_vec, &goal);
                }
            }
        }
        else if (result == 2 && !(flags & 4))
        {
            g_Player->do_graze(p);
            flags |= 4;
        }
        return result;
    }
    return 0;
}

// FUNCTION: TH16 0x412670
void Bullet::sub_412670()
{
    if (state == BULLET_STATE_FREE)
    {
        return;
    }
    state = BULLET_STATE_FREE;
    timer_144c.reset();
    timer_1460.reset();
    timer_1420.reset();
    timer_1434.reset();
    flags &= ~0x341;
    unk_c7c = 0;
    unk_c4c = 0;
    g_BulletManager->freelist_head.insert_after(&freelist_node);
    tick_list_node.unlink_inline();
}

// TODO: ours realigns the frame (and esp, -8), places the free path at
// the end and orders the half-step moves differently.
// FUNCTION: TH16 0x411e70
i32 Bullet::on_tick()
{
    timer_1460.tick();
    if (flags & 8)
    {
    die:
        sub_412670();
        return -1;
    }
    if (active_ex_flags & 0x400000)
    {
        scale = scale_i.step();
        if (scale_i.end_time == 0)
        {
            active_ex_flags &= ~0x400000;
            if (scale == 1.0f)
            {
                flags &= ~BULLET_FLAG_SCALED;
            }
        }
    }
    switch (state)
    {
    case 2:
        pos = pos + velocity * g_game_speed * 0.5f;
        if (timer_144c.current >= 8 && sub_4124b0(0) == 1)
        {
            break;
        }
        if (vm0.int_vars[0] == 0)
        {
            break;
        }
        state = 1;
    case 1:
        do
        {
            if (!(active_ex_flags & 0x4000000))
            {
                run_ex();
            }
            if (active_ex_flags == 0)
            {
                break;
            }
            i32 done = 0;
            if (active_ex_flags & 1)
            {
                done = step_ex_00();
            }
            if (active_ex_flags & 4)
            {
                done += step_ex_02();
            }
            if (active_ex_flags & 0x200000)
            {
                done += step_ex_21();
            }
            if (active_ex_flags & 8)
            {
                done += step_ex_03();
            }
            if (active_ex_flags & 0x10)
            {
                done += step_ex_04();
            }
            if (active_ex_flags & 0x40)
            {
                done += step_ex_06();
            }
            if (active_ex_flags & 0x20000)
            {
                done += step_ex_17();
            }
            if (active_ex_flags & 0x80000)
            {
                done += step_ex_19();
            }
            if (active_ex_flags & 0x100)
            {
                done += step_ex_08();
            }
            if (active_ex_flags & 0x80000000)
            {
                if (ex_state[5].timer.current <= 0)
                {
                    active_ex_flags ^= 0x80000000;
                    done++;
                }
                else
                {
                    ex_state[5].timer--;
                }
            }
            if (active_ex_flags & 0x4000000)
            {
                if (ex_state[13].timer.current <= 0)
                {
                    flags &= ~BULLET_FLAG_NO_DRAW;
                    active_ex_flags ^= 0x4000000;
                    done++;
                }
                else
                {
                    flags |= BULLET_FLAG_NO_DRAW;
                    ex_state[13].timer--;
                }
            }
            if (ex_invuln_remaining_frames != 0)
            {
                ex_invuln_remaining_frames--;
            }
            if (done == 0)
            {
                break;
            }
        } while (1);
        if (!(flags & BULLET_FLAG_NO_DRAW))
        {
            pos += velocity * g_game_speed;
            sub_4124b0(0);
        }
        break;
    case 3:
        pos = pos + velocity * g_game_speed * 0.5f;
        break;
    case 5:
        if (timer_144c.current < 3)
        {
            break;
        }
        if (timer_144c.current == 3)
        {
            vm0.interrupt_out_of_line(1);
            if (cancel_script >= 0)
            {
                AnmVm *vm = g_BulletManager->bullet_anm->create_vm(cancel_script, &pos, 0.0f, -1, 0).find_or_clear();
                D3DXVECTOR3 goal = velocity * g_game_speed * 10.0f;
                vm->set_pos_time(30, 6, &g_zero_vec, &goal);
            }
        }
        pos = pos + velocity * g_game_speed * 0.5f;
        break;
    }
    if (vm_sprite(&vm0) != NULL)
    {
        if (active_ex_flags & 0x1000)
        {
            step_ex_12();
        }
        if (!(active_ex_flags & 0x100) && unk_c58 < 1)
        {
            if (outside_range(pos.x, vm_sprite(&vm0)->sprite_width * scale, -192.0f, 192.0f) ||
                outside_range(pos.y, vm_sprite(&vm0)->sprite_height * scale, -64.0f, 480.0f))
            {
                goto die;
            }
        }
    }
    if (ex_invuln_remaining_frames != 0)
    {
        ex_invuln_remaining_frames--;
    }
    if (unk_c58 > 0)
    {
        unk_c58--;
    }
    if (!(flags & BULLET_FLAG_NO_DRAW) && vm0.run())
    {
        goto die;
    }
    if (vm1.flags_lo & 1)
    {
        vm1.run();
    }
    return 0;
}

// TODO: ours realigns the frame (and esp, -8) for corner, folds the
// timer decrement's multiply by 1.0f, and adds pos.x + half the other way.
// FUNCTION: TH16 0x4162d0
i32 Bullet::step_ex_08()
{
    ex_state[11].timer.decrement(1.0f);
    if (ex_state[11].ints[0] != 0 &&
        (outside_range(pos.x, vm_sprite(&vm0)->sprite_width, -192.0f, 192.0f) ||
         outside_range(pos.y, vm_sprite(&vm0)->sprite_height, 0.0f, 448.0f)))
    {
        D3DXVECTOR3 dir;
        D3DXVECTOR2 corner;
        bullet_sincosmul_2(&dir, angle, 1.0f);
        corner.x = (-384.0f - vm_sprite(&vm0)->sprite_width) * 0.5f - pos.x;
        corner.y = 224.0f - (vm_sprite(&vm0)->sprite_height + 448.0f) * 0.5f - pos.y;
        D3DXVec2Normalize(&corner, &corner);
        f32 cross_0 = dir.x * corner.y - dir.y * corner.x;
        f32 dot_0 = dir.y * corner.y + dir.x * corner.x;
        corner.x = (vm_sprite(&vm0)->sprite_width + 384.0f) * 0.5f - pos.x;
        corner.y = 224.0f - (vm_sprite(&vm0)->sprite_height + 448.0f) * 0.5f - pos.y;
        D3DXVec2Normalize(&corner, &corner);
        f32 cross_1 = dir.x * corner.y - dir.y * corner.x;
        f32 dot_1 = dir.y * corner.y + dir.x * corner.x;
        corner.x = (-384.0f - vm_sprite(&vm0)->sprite_width) * 0.5f - pos.x;
        corner.y = (vm_sprite(&vm0)->sprite_height + 448.0f) * 0.5f + 224.0f - pos.y;
        D3DXVec2Normalize(&corner, &corner);
        f32 cross_2 = dir.x * corner.y - dir.y * corner.x;
        f32 dot_2 = dir.y * corner.y + dir.x * corner.x;
        corner.x = (vm_sprite(&vm0)->sprite_width + 384.0f) * 0.5f - pos.x;
        corner.y = (vm_sprite(&vm0)->sprite_height + 448.0f) * 0.5f + 224.0f - pos.y;
        D3DXVec2Normalize(&corner, &corner);
        f32 cross_3 = dir.x * corner.y - dir.y * corner.x;
        f32 dot_3 = dir.y * corner.y + dir.x * corner.x;
        f32 best_left = -999.0f;
        if (0.0f >= cross_0 && dot_0 > best_left && dot_0 >= 0.0f)
        {
            best_left = dot_0;
        }
        if (0.0f >= cross_1 && dot_1 > best_left && dot_1 >= 0.0f)
        {
            best_left = dot_1;
        }
        if (0.0f >= cross_2 && dot_2 > best_left && dot_2 >= 0.0f)
        {
            best_left = dot_2;
        }
        if (0.0f >= cross_3 && dot_3 > best_left && dot_3 >= 0.0f)
        {
            best_left = dot_3;
        }
        f32 best_right = -999.0f;
        if (cross_0 >= 0.0f && dot_0 > best_right && dot_0 >= 0.0f)
        {
            best_right = dot_0;
        }
        if (cross_1 >= 0.0f && dot_1 > best_right && dot_1 >= 0.0f)
        {
            best_right = dot_1;
        }
        if (cross_2 >= 0.0f && dot_2 > best_right && dot_2 >= 0.0f)
        {
            best_right = dot_2;
        }
        if (cross_3 >= 0.0f && dot_3 > best_right && dot_3 >= 0.0f)
        {
            best_right = dot_3;
        }
        if (-998.0f > best_left || -998.0f > best_right)
        {
            active_ex_flags ^= 0x100;
            return 1;
        }
    }
    if (ex_state[11].timer.current <= 0)
    {
        active_ex_flags ^= 0x100;
        return 1;
    }
    return 0;
}
