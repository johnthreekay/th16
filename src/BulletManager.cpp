#include <math.h>
#include <string.h>

#include "AnmManager.h"
#include "BulletManager.h"
#include "Bomb.h"
#include "Collision.h"
#include "Enemy.h"
#include "Item.h"
#include "Player.h"
#include "Rng.h"
#include "SoundManager.h"
#include "Spellcard.h"
#include "GameErrorContext.h"
#include "GameThread.h"
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

// TODO: the original wraps a plain call in push ecx/pop ecx; ours tail-calls.
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
            g_BulletManager->anm_ids[bullet->index] =
                g_BulletManager->bullet_anm->create_vm(bullet->cancel_script, &bullet->pos, 0.0f, -1, 0);
        }
        g_SoundManager.play_sound_at_position(0x47, bullet->pos.x);
        gen_items_from_cancel(&bullet->pos, mode);
    }
    D3DXVECTOR3 delta = bullet->velocity * g_game_speed * 0.5f;
    bullet->pos.x = bullet->pos.x + delta.x;
    bullet->pos.y = bullet->pos.y + delta.y;
    bullet->pos.z = bullet->pos.z + delta.z;
    bullet->state = 4;
    bullet->timer_144c.reset();
    return 0;
}

// TODO: the original loads the ANM file before pushing create_vm's
// arguments and adds pos.x + delta.x with the operands swapped.
// FUNCTION: TH16 0x416840
i32 Bullet::cancel(i32 mode)
{
    return cancel_bullet(this, mode);
}

// TODO: as Bullet::cancel (create_vm argument order, vector add operands).
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
    f32 dy = bullet->pos.y - pos->y;
    f32 dx = bullet->pos.x - pos->x;
    return dy * dy + dx * dx <= r * r;
}

// TODO: the original does not thread the jump after the iterator's NULL
// entry, computes the y distance first and keeps 4 more frame bytes in the
// bomb version.
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

// TODO: as cancel_radius.
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

// TODO: the original frame has 4 more (unused) bytes and saves edi up
// front instead of around the mode 5 branch.
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
            g_ItemManager->spawn_item(10, pos, 0, g_replay_safe_rng.randf_neg_to(ZUN_PI / 18.0f) - ZUN_PI / 2.0f,
                                      2.2f, 0, 0);
        }
    }
    else if (mode == 5)
    {
        if (mgr->bomb_cancel_count_multiple_of(3))
        {
            g_ItemManager->spawn_item(16, pos, 0, g_replay_safe_rng.randf_neg_to(ZUN_PI / 18.0f) - ZUN_PI / 2.0f,
                                      2.2f, 0, 1);
        }
        g_BulletManager->bullet_count_canceled_by_bombs++;
    }
    else if (mode == 4)
    {
        g_ItemManager->spawn_item(16, pos, 0, g_replay_safe_rng.randf_neg_to(ZUN_PI / 18.0f) - ZUN_PI / 2.0f, 2.2f,
                                  0, 1);
        if (g_SubseasonBomb->in_use == 1)
        {
            g_ItemManager->spawn_item(g_SubseasonBomb->season_level + 8, pos, 0,
                                      g_replay_safe_rng.randf_neg_to(ZUN_PI / 18.0f) - ZUN_PI / 2.0f, 2.2f, 0, 0);
        }
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

// TODO: in the inlined timer tick the original adds the speed to
// current_f in xmm0 (ours adds current_f to the speed in xmm1).
// FUNCTION: TH16 0x414ec0
i32 Bullet::step_ex_00()
{
    if (ex_state[0].timer.current <= 16)
    {
        bullet_sincosmul(&velocity, angle, 5.0f - ex_state[0].timer.current_f * 5.0f / 16.0f + speed);
        ex_state[0].timer.tick();
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

// TODO: in the inlined timer tick the original keeps the frame in xmm0 on
// the unscaled path (ours shares xmm1 with the scaled path).
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
    ex_state[2].timer.tick();
    return 0;
}

// TODO: in the inlined timer tick the original loads current_f into xmm0
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
    ex_state[3].timer.tick();
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
