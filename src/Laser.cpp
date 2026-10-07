#include <math.h>
#include <string.h>

#include "AnmManager.h"
#include "CriticalSections.h"
#include "EffectManager.h"
#include "GameErrorContext.h"
#include "GameThread.h"
#include "Globals.h"
#include "Laser.h"
#include "Player.h"
#include "SoundManager.h"

// GLOBAL: TH16 0x4a6ee0
LaserManager *g_LaserManager;

// GLOBAL: TH16 0x49f2e0
BulletTypeInfo g_bullet_types[BULLET_TYPE_COUNT];

// This file's copy of ZunMath.h's sincosmul, which TH16 keeps once per
// object file. A static of its own so that it can be annotated. ZUN's laser
// code was one file; the laser methods that call it are kept here so that
// they call this copy (LTCG knows it leaves ecx and edx alone, which it
// would not assume for an external function).
// FUNCTION: TH16 0x43ad00
static void __fastcall laser_sincosmul(Float3 *dst, f32 angle, f32 radius)
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

// FUNCTION: TH16 0x42cb00
void LaserManager::destroy_all()
{
    LaserDataInf *laser = list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        laser->on_destroy();
        laser->unlink();
        delete laser;
        laser = next;
    }
    list_length = 0;
    list_tail = &list_head;
}

// FUNCTION: TH16 0x430e10
void LaserDataInf::get_point(f32 distance, Float3 *out)
{
}

// FUNCTION: TH16 0x430e20
void LaserDataInf::run_ex()
{
}

// FUNCTION: TH16 0x430e30
void LaserDataInf::method_8(i32 arg)
{
}

// FUNCTION: TH16 0x430e40
i32 LaserDataInf::initialize(void *params)
{
    return 0;
}

// FUNCTION: TH16 0x430e50
i32 LaserDataInf::on_tick()
{
    return 0;
}

// FUNCTION: TH16 0x430e60
i32 LaserDataInf::on_draw()
{
    return 0;
}

// FUNCTION: TH16 0x430e70
i32 LaserDataInf::on_destroy()
{
    unlink();
    return 0;
}

// FUNCTION: TH16 0x430e90
i32 LaserDataInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    return 0;
}

// FUNCTION: TH16 0x430ea0
i32 LaserDataInf::cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e)
{
    return 0;
}

// FUNCTION: TH16 0x430eb0
i32 LaserDataInf::cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d)
{
    return 0;
}

// FUNCTION: TH16 0x430ec0
i32 LaserDataInf::cancel(i32 mode, i32 b)
{
    return 0;
}

// FUNCTION: TH16 0x430ed0
i32 LaserDataInf::method_2c(i32 a, i32 b, i32 c, i32 d)
{
    return 0;
}

// FUNCTION: TH16 0x430ee0
i32 LaserDataInf::method_30(Float3 *pos, f32 radius)
{
    return 0;
}

// FUNCTION: TH16 0x430ef0
i32 LaserDataInf::check_graze_or_kill(i32 a)
{
    return 0;
}

// FUNCTION: TH16 0x430f00
i32 LaserDataInf::method_38()
{
    return 0;
}

// FUNCTION: TH16 0x430f10
i32 LaserDataInf::method_3c()
{
    return 0;
}

// FUNCTION: TH16 0x430f20
i32 LaserDataInf::method_40()
{
    return 0;
}

// FUNCTION: TH16 0x430f30
i32 LaserDataInf::method_44()
{
    return 0;
}

// FUNCTION: TH16 0x430f40
i32 LaserDataInf::method_48()
{
    return 0;
}

// FUNCTION: TH16 0x430f50
i32 LaserDataInf::method_4c()
{
    return 0;
}

// FUNCTION: TH16 0x430f60
i32 LaserDataInf::method_50()
{
    return 0;
}

// FUNCTION: TH16 0x430f70
i32 LaserDataInf::method_54()
{
    return 0;
}

// FUNCTION: TH16 0x430f80
i32 LaserDataInf::method_58()
{
    return 0;
}

// FUNCTION: TH16 0x430f90
i32 LaserDataInf::method_5c()
{
    return 0;
}

// FUNCTION: TH16 0x430fa0
i32 LaserDataInf::method_60()
{
    return 0;
}

// FUNCTION: TH16 0x430fb0
LaserDataInf *LaserDataInf::clone()
{
    return NULL;
}

// FUNCTION: TH16 0x430fc0
LaserDataInf::LaserDataInf()
{
    memset(this, 0, sizeof(*this));
    timer.reset();
}

// FUNCTION: TH16 0x431050
void LaserLineInf::get_point(f32 distance, Float3 *out)
{
    laser_sincosmul(out, angle, distance);
    *out += position;
}

// FUNCTION: TH16 0x4310b0
LaserDataInf *LaserLineInf::clone()
{
    LaserLineInf *copy = new LaserLineInf();
    memcpy(copy, this, sizeof(LaserLineInf));
    return copy;
}

// FUNCTION: TH16 0x431130
LaserLineInf::LaserLineInf()
{
}

// FUNCTION: TH16 0x4311f0
void LaserCurveInf::get_point(f32 distance, Float3 *out)
{
    laser_sincosmul(out, angle, distance);
    *out += position;
}

// FUNCTION: TH16 0x431250
void LaserInfiniteInf::get_point(f32 distance, Float3 *out)
{
    laser_sincosmul(out, angle, distance);
    *out += position;
}

// FUNCTION: TH16 0x4312b0
void LaserBeamInf::get_point(f32 distance, Float3 *out)
{
    laser_sincosmul(out, angle, distance);
    *out += position;
}

// FUNCTION: TH16 0x431310
void LaserBeamInf::method_8(i32 arg)
{
    inner.flag_38 = arg;
}

// TODO: the original reserves an unused stack slot (push ecx) and saves esi
// on entry; ours saves esi only on the success path.
// FUNCTION: TH16 0x431330
i32 LaserManager::initialize()
{
    bullet_anm = AnmManager::preload_anm(7, "bullet.anm");
    if (bullet_anm == NULL)
    {
        // "Enemy bullet data not found. The data is corrupted."
        g_GameErrorContext.log("\x93G\x92" "e\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82"
                               "\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82"
                               "\xdc\x82\xb7\r\n");
        return -1;
    }
    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x1b);
    on_tick = f;
    f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x23);
    on_draw = f;
    list_tail = &list_head;
    return 0;
}

LaserManager::LaserManager()
{
    memset(this, 0, sizeof(LaserManager));
    last_id = 0x10000;
    g_LaserManager = this;
}

// FUNCTION: TH16 0x4313b0
LaserManager::~LaserManager()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    destroy_all();
    g_LaserManager = NULL;
}

// FUNCTION: TH16 0x4314a0
LaserManager *LaserManager::create()
{
    LaserManager *mgr = new LaserManager();
    if (mgr->initialize() != 0)
    {
        delete mgr;
        return NULL;
    }
    return mgr;
}

// FUNCTION: TH16 0x431510
i32 LaserManager::on_tick_body()
{
    LaserDataInf *laser = list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        if (laser->pending_delete)
        {
            laser->pending_delete++;
            if (laser->pending_delete >= 2)
            {
                destroy(laser);
                laser = next;
                continue;
            }
        }
        if (laser->state == 1)
        {
            destroy(laser);
        }
        else if (laser->flag_3)
        {
            laser->check_graze_or_kill(1);
        }
        else if (laser->on_tick())
        {
            destroy(laser);
        }
        else
        {
            laser->timer.tick();
            laser->ticked = 1;
        }
        laser = next;
    }
    return 1;
}

// FUNCTION: TH16 0x4316b0
i32 __fastcall LaserManager::on_tick_callback(LaserManager *mgr)
{
    if (g_GameThread->flags.flag_0 | g_GameThread->flags.paused)
    {
        return 1;
    }
    if (g_GameThread->flags.flag_10)
    {
        return 1;
    }
    if (g_GameThread->flags.flag_1)
    {
        f32 speed = g_game_speed;
        g_game_speed = 0.0f;
        i32 result = mgr->on_tick_body();
        g_game_speed = speed;
        return result;
    }
    return mgr->on_tick_body();
}

// FUNCTION: TH16 0x431720
i32 __fastcall LaserManager::on_draw_callback(LaserManager *mgr)
{
    if (g_GameThread->flags.paused)
    {
        return 1;
    }
    LaserDataInf *laser = mgr->list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        if (laser->state != 1)
        {
            laser->on_draw();
        }
        laser = next;
    }
    return 1;
}

// TODO: the original reserves an unused stack slot (push ecx) and keeps the
// new laser in eax while linking it.
// FUNCTION: TH16 0x431760
i32 LaserManager::allocate_new_laser(i32 kind, void *params)
{
    LaserManager *mgr = g_LaserManager;
    if (mgr->list_length >= 0x200)
    {
        return 0;
    }
    mgr->last_id++;
    if (mgr->last_id < 0x10000)
    {
        mgr->last_id = 0x10000;
    }
    LaserDataInf *laser;
    switch (kind)
    {
    case LASER_LINE:
        laser = new LaserLineInf();
        break;
    case LASER_INFINITE:
        laser = new LaserInfiniteInf();
        break;
    case LASER_BEAM:
        laser = new LaserBeamInf();
        break;
    case LASER_CURVE:
        laser = new LaserCurveInf();
        break;
    default:
        return mgr->last_id;
    }
    laser->id = mgr->last_id;
    mgr->append(laser);
    laser->initialize(params);
    return mgr->last_id;
}

// FUNCTION: TH16 0x431860
LaserInfiniteInf::LaserInfiniteInf()
{
}

// FUNCTION: TH16 0x4318c0
LaserBeamInf::LaserBeamInf()
{
}

// FUNCTION: TH16 0x431900
LaserCurveInf::LaserCurveInf()
{
}

// TODO: the original has an 8-byte frame (sub esp, 8) where ours has 4.
// FUNCTION: TH16 0x431950
HARNESS_CALLED i32 LaserManager::cancel_in_rectangle(Float3 *a, Float3 *b, f32 angle, i32 mode, i32 e)
{
    LaserManager *mgr = g_LaserManager;
    LaserDataInf *laser = mgr->list_head.next;
    i32 count = 0;
    mgr->cancel_pos = *a;
    mgr->cancel_pos_2 = *b;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        if (laser->state != 1 && laser->ticked)
        {
            count += laser->cancel_as_bomb_rectangle(a, b, angle, mode, e);
        }
        laser = next;
    }
    return count;
}

// FUNCTION: TH16 0x4319e0
i32 LaserManager::cancel_all()
{
    LaserDataInf *laser = g_LaserManager->list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        laser->timer_5a0.reset();
        laser->countdown_5c8 = 0;
        laser->cancel(1, 0);
        laser = next;
    }
    return 0;
}

// TODO: the original has an 8-byte frame (sub esp, 8) where ours has 4.
// FUNCTION: TH16 0x431a70
HARNESS_CALLED i32 LaserManager::cancel_in_radius(Float3 *pos, f32 radius, i32 c, i32 d)
{
    LaserManager *mgr = g_LaserManager;
    LaserDataInf *laser = mgr->list_head.next;
    LaserDataInf *next;
    i32 count = 0;
    mgr->cancel_pos = *pos;
    for (; laser != NULL; laser = next)
    {
        next = laser->next;
        if (laser->state == 1)
        {
            continue;
        }
        count += laser->cancel_as_bomb_circle(pos, radius, c, d);
    }
    return count;
}

// FUNCTION: TH16 0x431af0
HARNESS_CALLED i32 LaserManager::clear_all(i32 mode, i32 b)
{
    LaserDataInf *laser = g_LaserManager->list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        if (laser->state != 1)
        {
            laser->cancel(mode, b);
        }
        laser = next;
    }
    return 1;
}

// FUNCTION: TH16 0x411860
LaserInfiniteInner::LaserInfiniteInner()
{
    memset(this, 0, sizeof(LaserInfiniteInner));
    speed = 8.0f;
}

// FUNCTION: TH16 0x433720
i32 LaserLineInf::on_draw()
{
    i32 i = 0;
    vm_92c.pos = position;
    f32 rotation = angle + ZUN_PI / 2;
    while (rotation > ZUN_PI)
    {
        rotation -= ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    while (rotation < -ZUN_PI)
    {
        rotation += ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    AnmVm *vm = &vm_92c;
    vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
    vm->rotation.z = rotation;
    g_AnmManager->draw_vm(vm);
    Float3 *tip = &vm_1524.pos;
    laser_sincosmul(tip, angle, unk_70);
    tip->z = 0.0f;
    tip->x += position.x;
    tip->y += position.y;
    g_AnmManager->draw_vm(&vm_1524);
    if (unk_7c == 0.0f)
    {
        vm_f28.pos = position;
        g_AnmManager->draw_vm(&vm_f28);
    }
    return 0;
}

// An et_ex step: moves the curve's origin by ex_state[1]'s velocity (scaled
// by the game speed) and turns it to face its direction of motion, until
// the step's time runs out.
// TODO: the original adds and stores the velocity one component at a time and reloads unk_60.x for the fabsf test.
// FUNCTION: TH16 0x4395b0
i32 LaserCurveInf::method_3c()
{
    BulletExState *st = &ex_state[1];
    if (st->timer.current >= st->ints[0])
    {
        ex_flags &= ~4;
        return 1;
    }
    length += st->floats[0] * g_game_speed;
    unk_60 += *(Float3 *)&st->floats[5] * g_game_speed;
    if (fabsf(unk_60.x) > 0.0001f || fabsf(unk_60.y) > 0.0001f)
    {
        angle = atan2(unk_60.y, unk_60.x);
    }
    st->timer.tick();
    return 0;
}

// An et_ex step: turns the curve by ex_state[2]'s angular speed and grows
// it, until the step's time runs out.
// TODO: the original stores the new angle after loading floats[0] (scheduling).
// FUNCTION: TH16 0x439460
i32 LaserCurveInf::method_40()
{
    BulletExState *st = &ex_state[2];
    if (st->timer.current >= st->ints[0])
    {
        ex_flags &= ~8;
        return 1;
    }
    i32 i = 0;
    f32 a = st->floats[1] * g_game_speed + angle;
    while (a > ZUN_PI)
    {
        a -= ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    while (a < -ZUN_PI)
    {
        a += ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    angle = a;
    length += st->floats[0] * g_game_speed;
    laser_sincosmul(&unk_60, angle, length);
    st->timer.tick();
    return 0;
}

// The same et_ex step for straight lasers.
// TODO: as LaserCurveInf::method_44: the new angle in xmm0 (ours xmm1), ints[2] incremented later, current_f added into the speed register.
// FUNCTION: TH16 0x432c20
i32 LaserLineInf::method_44()
{
    f32 len;
    if (ex_state[3].timer.current >= ex_state[3].ints[0])
    {
        if (inner.shot_transform_sfx >= 0)
        {
            g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
        }
        f32 a = ex_state[3].floats[1] + angle;
        ex_state[3].ints[2]++;
        len = ex_state[3].floats[0];
        length = len;
        angle = a;
        ex_state[3].timer.reset();
        if (ex_state[3].ints[2] >= ex_state[3].ints[1])
        {
            laser_sincosmul(&unk_60, a, len);
            ex_flags &= ~0x10;
            return 1;
        }
    }
    else
    {
        len = length - ex_state[3].timer.current_f * length / ex_state[3].ints[0];
    }
    laser_sincosmul(&unk_60, angle, len);
    ex_state[3].timer.tick();
    return 0;
}

// An et_ex step: retracts the curve over ex_state[3]'s time, then turns it
// and gives it a new length; after ints[1] rounds the step ends.
// TODO: the original keeps the new angle in xmm0 (ours xmm1), increments ints[2] later and adds current_f into the speed register in the timer tick.
// FUNCTION: TH16 0x4392c0
i32 LaserCurveInf::method_44()
{
    f32 len;
    if (ex_state[3].timer.current >= ex_state[3].ints[0])
    {
        if (inner.shot_transform_sfx >= 0)
        {
            g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
        }
        f32 a = ex_state[3].floats[1] + angle;
        ex_state[3].ints[2]++;
        len = ex_state[3].floats[0];
        length = len;
        angle = a;
        ex_state[3].timer.reset();
        if (ex_state[3].ints[2] >= ex_state[3].ints[1])
        {
            laser_sincosmul(&unk_60, a, len);
            ex_flags &= ~0x10;
            return 1;
        }
    }
    else
    {
        len = length - ex_state[3].timer.current_f * length / ex_state[3].ints[0];
    }
    laser_sincosmul(&unk_60, angle, len);
    ex_state[3].timer.tick();
    return 0;
}

// Cancels the laser: a cancel effect and cancel items every 16 units along
// it. Returns the number of points.
// TODO: the original builds the first point as one vector copied to pos and the effect copy, and copies it again at the loop end; ours copies it inside the inlined create_vm.
// FUNCTION: TH16 0x434cd0
i32 LaserLineInf::cancel(i32 mode, i32 b)
{
    if (b != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    f32 dist = 8.0f;
    i32 count = 0;
    Float3 step;
    Float3 pos;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    pos = step + position;
    step.x += step.x;
    step.y += step.y;
    while (unk_70 > dist + 8.0f)
    {
        D3DXVECTOR3 effect_pos = pos;
        count++;
        if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
        {
            AnmLoaded *anm = g_BulletManager->bullet_anm;
            anm->create_vm_inline(inner.bullet_color * 2 + 0xd1, &effect_pos, 0.0f, -1);
        }
        else if (bullet_type <= 0x1f || bullet_type == 0x1b)
        {
            g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x101, &pos, 0.0f, -1, 0);
        }
        else if (bullet_type <= 0x21)
        {
            g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x119, &pos, 0.0f, -1, 0);
        }
        gen_items_from_cancel(&pos, mode);
        pos += step;
        dist += 16.0f;
    }
    state = 1;
    return count;
}

// Cancels the laser like LaserLineInf::cancel, but only the points on screen
// get an effect and items.
// TODO: the original doubles step.x after loading position (scheduling) and stores step.z = 0 late from a second zero register.
// FUNCTION: TH16 0x436c70
i32 LaserInfiniteInf::cancel(i32 mode, i32 b)
{
    if (b != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    f32 dist = 8.0f;
    i32 count = 0;
    Float3 step;
    Float3 pos;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    pos = step + position;
    step.x += step.x;
    step.y += step.y;
    while (unk_70 > dist + 8.0f)
    {
        count++;
        if (!(pos.x + 16.0f <= -192.0f || pos.x - 16.0f >= 192.0f || pos.y + 16.0f <= 0.0f || pos.y - 16.0f >= 448.0f))
        {
            if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
            {
                g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0xd1, &pos, 0.0f, -1, 0);
            }
            else if (bullet_type <= 0x1f || bullet_type == 0x1b)
            {
                g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x101, &pos, 0.0f, -1, 0);
            }
            else if (bullet_type <= 0x21)
            {
                g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x119, &pos, 0.0f, -1, 0);
            }
            gen_items_from_cancel(&pos, mode);
        }
        pos += step;
        dist += 16.0f;
    }
    state = 1;
    return count;
}

// Cancels the points (every 16 units) inside a bomb's circle, then cuts the
// laser: a hit head moves its start forward, the first hit run ends it, and
// every later unhit run becomes a new laser. Returns the number of points
// hit.
// TODO: register allocation differs throughout (the original keeps center in ebx and count in memory) and the run loops are laid out differently.
// FUNCTION: TH16 0x434730
i32 LaserLineInf::cancel_as_bomb_circle(Float3 *center, f32 radius, i32 mode, i32 d)
{
    if (d != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    Float3 origin = position;
    i32 count = 0;
    f32 dist = 8.0f;
    u8 hit[0x100];
    memset(hit, 0, sizeof(hit));
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    Float3 pos;
    pos = position + step;
    pos.z = 0.0f;
    step.x += step.x;
    step.y += step.y;
    step.z += step.z;
    radius = radius * radius;
    i32 i;
    for (i = 0; unk_70 >= dist + 8.0f; i++)
    {
        if (!((center->x - pos.x) * (center->x - pos.x) + (center->y - pos.y) * (center->y - pos.y) > radius))
        {
            count++;
            hit[i] = 1;
            gen_items_from_cancel(&pos, mode);
            if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
            {
                g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0xd1, &pos, 0.0f, -1, 0);
            }
            else if (bullet_type <= 0x1f || bullet_type == 0x1b)
            {
                g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x101, &pos, 0.0f, -1, 0);
            }
            else if (bullet_type <= 0x21)
            {
                g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x119, &pos, 0.0f, -1, 0);
            }
        }
        pos += step;
        dist += 16.0f;
    }
    if (count != 0)
    {
        if (count >= i)
        {
            pending_delete = 1;
            return count;
        }
        i32 j;
        for (j = 0; j < i; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            position += step * (f32)j;
            unk_70 -= (f32)j * 16.0f;
            if (!(unk_70 > 24.0f))
            {
                pending_delete = 1;
                return count;
            }
            inner.laser_new_arg_2 = unk_70;
            unk_7c = (f32)j * 16.0f;
        }
        i32 run = 0;
        if (j < i)
        {
            for (; j < i; j++, run++)
            {
                if (hit[j])
                {
                    break;
                }
            }
            if (j < i)
            {
                f32 len = (f32)run * 16.0f;
                inner.laser_new_arg_2 -= unk_70 - len;
                unk_70 = len;
                if (24.0f > len)
                {
                    pending_delete = 1;
                }
                do
                {
                    if (hit[j])
                    {
                        j++;
                        continue;
                    }
                    i32 start = j;
                    run = 0;
                    while (!hit[j])
                    {
                        j++;
                        run++;
                        if (j >= i)
                        {
                            break;
                        }
                    }
                    LaserLineInner params = inner;
                    params.laser_new_arg_2 = params.laser_new_arg_1 = (f32)run * 16.0f;
                    if (params.laser_new_arg_1 > 24.0f)
                    {
                        params.start_pos = origin + step * (f32)start;
                        g_LaserManager->allocate_new_laser(LASER_LINE, &params);
                    }
                } while (j < i);
            }
        }
    }
    return count;
}

// Cancels the points (every 16 units) inside a bomb's circle. A hit head
// shortens the laser to nothing, otherwise it ends at the first hit run;
// every later unhit run that starts on screen becomes a straight laser.
// Returns the number of points hit.
// TODO: the original zeroes i (ebx) before the memset and stores step.z first; the run loops' register use and the params copy differ.
// FUNCTION: TH16 0x436670
i32 LaserInfiniteInf::cancel_as_bomb_circle(Float3 *center, f32 radius, i32 mode, i32 d)
{
    if (d != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    Float3 origin = position;
    i32 count = 0;
    f32 dist = 8.0f;
    u8 hit[0x100];
    memset(hit, 0, sizeof(hit));
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    Float3 pos;
    pos = position + step;
    pos.z = 0.0f;
    step.x += step.x;
    step.y += step.y;
    step.z += step.z;
    radius = radius * radius;
    i32 i;
    for (i = 0; unk_70 > dist + 8.0f; i++)
    {
        if (!((center->x - pos.x) * (center->x - pos.x) + (center->y - pos.y) * (center->y - pos.y) > radius))
        {
            count++;
            hit[i] = 1;
            gen_items_from_cancel(&pos, mode);
            if (!(pos.x + 32.0f <= -192.0f || pos.x - 32.0f >= 192.0f || pos.y + 32.0f <= 0.0f ||
                  pos.y - 32.0f >= 448.0f))
            {
                if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
                {
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0xd1, &pos, 0.0f, -1, 0);
                }
                else if (bullet_type <= 0x1f || bullet_type == 0x1b)
                {
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x101, &pos, 0.0f, -1, 0);
                }
                else if (bullet_type <= 0x21)
                {
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x119, &pos, 0.0f, -1, 0);
                }
            }
        }
        pos += step;
        dist += 16.0f;
    }
    if (count != 0)
    {
        i32 j;
        for (j = 0; j < i; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            unk_70 = 0.0f;
        }
        else
        {
            i32 run = 0;
            for (; j < i; j++, run++)
            {
                if (hit[j])
                {
                    break;
                }
            }
            if (j < i)
            {
                unk_70 = (f32)run * 16.0f;
            }
        }
        while (j < i)
        {
            if (hit[j])
            {
                j++;
                continue;
            }
            i32 run = 0;
            i32 start = j;
            while (!hit[j])
            {
                j++;
                run++;
                if (j >= i)
                {
                    break;
                }
            }
            f32 start_f = (f32)start;
            pos = origin + step * start_f;
            if (!(pos.x + 32.0f <= -192.0f || pos.x - 32.0f >= 192.0f || pos.y + 32.0f <= 0.0f ||
                  pos.y - 32.0f >= 448.0f))
            {
                LaserLineInner params;
                params.start_pos = pos;
                params.speed = 8.0f;
                params.bullet_type = inner.type;
                params.bullet_color = inner.color;
                params.laser_new_arg_2 = params.laser_new_arg_1 = (f32)run * 16.0f;
                params.ang_aim = angle;
                params.laser_new_arg_4 = width;
                params.laser_new_arg_3 = inner.laser_new_arg_2 - start_f * 16.0f;
                g_LaserManager->allocate_new_laser(LASER_LINE, &params);
            }
        }
    }
    return count;
}

// FUNCTION: TH16 0x4357a0
i32 LaserInfiniteInf::on_draw()
{
    i32 i = 0;
    vm_950.pos = position;
    f32 rotation = angle + ZUN_PI / 2;
    while (rotation > ZUN_PI)
    {
        rotation -= ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    while (rotation < -ZUN_PI)
    {
        rotation += ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    AnmVm *vm = &vm_950;
    vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
    vm->rotation.z = rotation;
    g_AnmManager->draw_vm(vm);
    if (unk_7c == 0.0f)
    {
        vm_f4c.pos = position;
        g_AnmManager->draw_vm(&vm_f4c);
    }
    return 0;
}

// Hits or grazes the player: a hit cancels the laser around the player, a
// graze counts every third frame at the point of the laser nearest the
// player.
// TODO: the original adds position.x to the loaded start.x (operand order) and calls cancel_as_bomb_rectangle without speculative devirtualization.
// FUNCTION: TH16 0x433510
i32 LaserLineInf::check_graze_or_kill(i32 graze_only)
{
    if (unk_70 > 16.0f && width > 3.0f)
    {
        Float3 start;
        if (!(inner.flags & 2))
        {
            laser_sincosmul(&start, angle, unk_70 / 10.0f);
            start.x += position.x;
            start.y = position.y + start.y;
            start.z = position.z + start.z;
        }
        else
        {
            start = position;
        }
        f32 length = unk_70;
        if (!(inner.flags & 2))
        {
            length = length * 4.0f / 5.0f;
        }
        f32 w = width;
        if (32.0f > w)
        {
            w = w * 0.5f;
        }
        else
        {
            w = w - (w + 16.0f) * 0.5f;
        }
        i32 result = g_Player->check_hit_rotated_rect(&start, angle, w, length, graze_only);
        if (result == 1)
        {
            Float3 size(32.0f, 32.0f, 0.0f);
            cancel_as_bomb_rectangle(&g_Player->inner.pos, &size, 0.0f, 0, 1);
            return 0;
        }
        if (result == 2)
        {
            if (timer_2c.current % 3 == 0)
            {
                f32 x;
                f32 y;
                line_intersection(&x, &y, start.x, start.y, angle, g_Player->inner.pos.x, g_Player->inner.pos.y,
                                  normalize_angle(angle + ZUN_PI / 2));
                start.x = x;
                start.y = y;
                g_Player->do_graze(&start);
            }
            timer_2c++;
        }
    }
    return 0;
}

// The same for infinite lasers, once they are out (states 2 and 4).
// TODO: the original keeps angle in xmm3 across normalize_angle and calls cancel_as_bomb_rectangle without speculative devirtualization.
// FUNCTION: TH16 0x435610
i32 LaserInfiniteInf::check_graze_or_kill(i32 graze_only)
{
    if ((state == 4 || state == 2) && unk_70 > 16.0f)
    {
        Float3 start = position;
        f32 w = width;
        if (32.0f > w)
        {
            w = w * 0.5f;
        }
        else
        {
            w = w - (w + 16.0f) / 3.0f;
        }
        i32 result = g_Player->check_hit_rotated_rect(&start, angle, w, unk_70 * 0.9f, graze_only);
        if (result == 1)
        {
            Float3 size(32.0f, 32.0f, 0.0f);
            cancel_as_bomb_rectangle(&g_Player->inner.pos, &size, 0.0f, 0, 1);
            return 0;
        }
        if (result == 2)
        {
            if (timer_2c.current % 3 == 0)
            {
                f32 x;
                f32 y;
                line_intersection(&x, &y, start.x, start.y, angle, g_Player->inner.pos.x, g_Player->inner.pos.y,
                                  normalize_angle(angle + ZUN_PI / 2));
                start.x = x;
                start.y = y;
                g_Player->do_graze(&start);
            }
            timer_2c++;
        }
    }
    return 0;
}

// The same for curvy lasers, piece by piece past the first 16 units; one
// graze per frame at most.
// TODO: the original adds segment->pos.z to the loaded mid.z (operand order) and calls cancel_as_bomb_rectangle without speculative devirtualization.
// FUNCTION: TH16 0x437cf0
i32 LaserCurveInf::check_graze_or_kill(i32 graze_only)
{
    i32 grazed = 0;
    f32 dist = 0.0f;
    Float3 graze_pos;
    LaserCurveSegment *segment = (LaserCurveSegment *)unk_1524;
    for (i32 i = 0; i < inner.segment_count - 1; i++, segment++)
    {
        Float3 mid;
        laser_sincosmul(&mid, segment->angle, segment->length * 0.5f);
        mid += segment->pos;
        dist += segment->length;
        if (dist >= 16.0f)
        {
            i32 result = g_Player->check_hit_rotated_rect(&mid, segment->angle, width * 0.5f, segment->length, graze_only);
            if (result == 1)
            {
                Float3 size(32.0f, 32.0f, 0.0f);
                cancel_as_bomb_rectangle(&g_Player->inner.pos, &size, 0.0f, 0, 1);
            }
            else if (result == 2 && !grazed && timer_2c.current % 3 == 0)
            {
                graze_pos = mid;
                grazed = 1;
            }
        }
    }
    if (grazed)
    {
        g_Player->do_graze(&graze_pos);
    }
    timer_2c.tick();
    return 0;
}

// cancel_as_bomb_circle for a bomb's rectangle (center, size, rotated by
// rect_angle): the points are tested in the rectangle's frame.
// TODO: register allocation differs throughout (the original keeps this in esi and copies center and size to locals first).
// FUNCTION: TH16 0x433860
i32 LaserLineInf::cancel_as_bomb_rectangle(Float3 *center, Float3 *size, f32 rect_angle, i32 mode, i32 e)
{
    if (e != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    Float3 origin = position;
    i32 count = 0;
    f32 dist = 8.0f;
    u8 hit[0x100];
    memset(hit, 0, sizeof(hit));
    f32 dx = position.x - center->x;
    f32 dy = position.y - center->y;
    f32 neg_angle = -rect_angle;
    f32 s = zun_sinf(neg_angle);
    f32 c = zun_cosf(neg_angle);
    f32 local_x = dx * c - dy * s;
    f32 local_y = dy * c + dx * s;
    i32 n = 0;
    f32 local_angle = angle - rect_angle;
    while (local_angle > ZUN_PI)
    {
        local_angle -= ZUN_2PI;
        if (n++ > 32)
        {
            break;
        }
    }
    while (local_angle < -ZUN_PI)
    {
        local_angle += ZUN_2PI;
        if (n++ > 32)
        {
            break;
        }
    }
    Float3 local_step;
    laser_sincosmul(&local_step, local_angle, 8.0f);
    local_y += local_step.y;
    local_step.z = 0.0f;
    local_step.y += local_step.y;
    f32 half_w = size->x * 0.5f;
    f32 half_h = size->y * 0.5f;
    local_x += local_step.x;
    local_step.x += local_step.x;
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    Float3 pos;
    pos = position + step;
    pos.z = 0.0f;
    step.x += step.x;
    step.y += step.y;
    step.z += step.z;
    i32 i;
    for (i = 0; unk_70 >= dist + 8.0f; i++)
    {
        if (!(-half_w > local_x || local_x > half_w || -half_h > local_y || local_y > half_h))
        {
            count++;
            hit[i] = 1;
            gen_items_from_cancel(&pos, mode);
            if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
            {
                AnmId id = g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0xd1, &pos, 0.0f, -1, 0);
                g_EffectManager->track_inline(id);
            }
            else if (bullet_type <= 0x1e)
            {
                g_EffectManager->track(
                    g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x101, &pos, 0.0f, -1, 0));
            }
            else if (bullet_type <= 0x21)
            {
                g_EffectManager->track(
                    g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x119, &pos, 0.0f, -1, 0));
            }
        }
        pos += step;
        local_x += local_step.x;
        local_y += local_step.y;
        dist += 16.0f;
    }
    if (count != 0)
    {
        if (count >= i)
        {
            pending_delete = 1;
            return count;
        }
        i32 j;
        for (j = 0; j < i; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            position += step * (f32)j;
            unk_70 -= (f32)j * 16.0f;
            if (!(unk_70 > 24.0f))
            {
                pending_delete = 1;
                return count;
            }
            inner.laser_new_arg_2 = unk_70;
            unk_7c = (f32)j * 16.0f;
        }
        i32 run = 0;
        if (j < i)
        {
            for (; j < i; j++, run++)
            {
                if (hit[j])
                {
                    break;
                }
            }
            if (j < i)
            {
                f32 len = (f32)run * 16.0f;
                inner.laser_new_arg_2 -= unk_70 - len;
                unk_70 = len;
                if (24.0f > len)
                {
                    pending_delete = 1;
                }
                do
                {
                    if (hit[j])
                    {
                        j++;
                        continue;
                    }
                    i32 start = j;
                    run = 0;
                    while (!hit[j])
                    {
                        j++;
                        run++;
                        if (j >= i)
                        {
                            break;
                        }
                    }
                    LaserLineInner params = inner;
                    params.laser_new_arg_2 = params.laser_new_arg_1 = (f32)run * 16.0f;
                    if (params.laser_new_arg_1 > 24.0f)
                    {
                        params.start_pos = origin + step * (f32)start;
                        g_LaserManager->allocate_new_laser(LASER_LINE, &params);
                    }
                } while (j < i);
            }
        }
    }
    return count;
}

// cancel_as_bomb_circle for a bomb's rectangle, tested in the rectangle's
// frame. The pieces after the first hit run become straight lasers.
// TODO: register allocation differs throughout, as in LaserLineInf::cancel_as_bomb_rectangle.
// FUNCTION: TH16 0x435880
i32 LaserInfiniteInf::cancel_as_bomb_rectangle(Float3 *center, Float3 *size, f32 rect_angle, i32 mode, i32 e)
{
    if (e != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    Float3 origin = position;
    i32 count = 0;
    f32 dist = 8.0f;
    u8 hit[0x100];
    memset(hit, 0, sizeof(hit));
    f32 dx = position.x - center->x;
    f32 dy = position.y - center->y;
    f32 neg_angle = -rect_angle;
    f32 s = zun_sinf(neg_angle);
    f32 c = zun_cosf(neg_angle);
    f32 local_x = dx * c - dy * s;
    f32 local_y = dy * c + dx * s;
    i32 n = 0;
    f32 local_angle = angle - rect_angle;
    while (local_angle > ZUN_PI)
    {
        local_angle -= ZUN_2PI;
        if (n++ > 32)
        {
            break;
        }
    }
    while (local_angle < -ZUN_PI)
    {
        local_angle += ZUN_2PI;
        if (n++ > 32)
        {
            break;
        }
    }
    Float3 local_step;
    laser_sincosmul(&local_step, local_angle, 8.0f);
    local_y += local_step.y;
    local_step.z = 0.0f;
    local_step.y += local_step.y;
    f32 half_w = size->x * 0.5f;
    f32 half_h = size->y * 0.5f;
    local_x += local_step.x;
    local_step.x += local_step.x;
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    Float3 pos;
    pos = position + step;
    pos.z = 0.0f;
    step.x += step.x;
    step.y += step.y;
    step.z += step.z;
    i32 i;
    for (i = 0; unk_70 >= dist + 8.0f; i++)
    {
        if (!(-half_w > local_x || local_x > half_w || -half_h > local_y || local_y > half_h))
        {
            count++;
            hit[i] = 1;
            gen_items_from_cancel(&pos, mode);
            if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
            {
                AnmId id = g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0xd1, &pos, 0.0f, -1, 0);
                g_EffectManager->track_inline(id);
            }
            else if (bullet_type <= 0x1f || bullet_type == 0x1b)
            {
                g_EffectManager->track(
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x101, &pos, 0.0f, -1, 0));
            }
            else if (bullet_type <= 0x21)
            {
                g_EffectManager->track(
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x119, &pos, 0.0f, -1, 0));
            }
        }
        pos += step;
        local_x += local_step.x;
        local_y += local_step.y;
        dist += 16.0f;
    }
    if (count != 0)
    {
        i32 j;
        for (j = 0; j < i; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            unk_70 = 0.0f;
        }
        else
        {
            i32 run = 0;
            for (; j < i; j++, run++)
            {
                if (hit[j])
                {
                    break;
                }
            }
            if (j < i)
            {
                unk_70 = (f32)run * 16.0f;
            }
        }
        while (j < i)
        {
            if (hit[j])
            {
                j++;
                continue;
            }
            i32 run = 0;
            i32 start = j;
            while (!hit[j])
            {
                j++;
                run++;
                if (j >= i)
                {
                    break;
                }
            }
            LaserLineInner params;
            params.speed = 8.0f;
            params.distance = 0.0f;
            params.shot_sfx = -1;
            params.shot_transform_sfx = -1;
            params.laser_new_arg_2 = params.laser_new_arg_1 = (f32)run * 16.0f;
            params.start_pos = origin + step * (f32)start;
            params.ang_aim = angle;
            params.bullet_type = inner.type;
            params.laser_new_arg_4 = width;
            params.bullet_color = inner.color;
            params.laser_new_arg_3 = inner.laser_new_arg_2 - (f32)start * 16.0f;
            params.flags ^= (params.flags ^ (inner.flags >> 1)) & 1;
            g_LaserManager->allocate_new_laser(LASER_LINE, &params);
        }
    }
    return count;
}
