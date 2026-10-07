#include <math.h>
#include <stddef.h>
#include <string.h>

#include "AnmManager.h"
#include "Collision.h"
#include "CriticalSections.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "Enemy.h"
#include "GameErrorContext.h"
#include "GameThread.h"
#include "Globals.h"
#include "Laser.h"
#include "Player.h"
#include "SoundManager.h"

static_assert(offsetof(LaserLineInner, ex) == 0x38, "LaserLineInner::ex");
static_assert(offsetof(LaserLineInner, shot_sfx) == 0x350, "LaserLineInner::shot_sfx");

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
    LaserLineInf *copy = new LaserLineInf(LaserLineInf::InlineCtor());
    memcpy(copy, this, sizeof(LaserLineInf));
    return copy;
}

// Called out of line everywhere but in clone.
// FUNCTION: TH16 0x431130
DECOMP_NOINLINE LaserLineInf::LaserLineInf()
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
    if (g_GameThread->flags.flag_0 | g_GameThread->flags.loading)
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
    if (g_GameThread->flags.loading)
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

// Each kind links and initializes its laser itself; the compiler merges the
// four copies.
// TODO: the original reserves an unused stack slot (push ecx).
// FUNCTION: TH16 0x431760
DECOMP_NOINLINE i32 LaserManager::allocate_new_laser(i32 kind, void *params)
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
        laser->id = mgr->last_id;
        mgr->append(laser);
        laser->initialize(params);
        break;
    case LASER_INFINITE:
        laser = new LaserInfiniteInf();
        laser->id = mgr->last_id;
        mgr->append(laser);
        laser->initialize(params);
        break;
    case LASER_BEAM:
        laser = new LaserBeamInf();
        laser->id = mgr->last_id;
        mgr->append(laser);
        laser->initialize(params);
        break;
    case LASER_CURVE:
        laser = new LaserCurveInf();
        laser->id = mgr->last_id;
        mgr->append(laser);
        laser->initialize(params);
        break;
    }
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
    Float3 v = *(Float3 *)&st->floats[5] * g_game_speed;
    D3DXVec3Add(&unk_60, &unk_60, &v);
    if (fabsf(unk_60.x) > 0.0001f || fabsf(unk_60.y) > 0.0001f)
    {
        angle = atan2(unk_60.y, unk_60.x);
    }
    st->timer.tick();
    return 0;
}

// An et_ex step: turns the curve by ex_state[2]'s angular speed and grows
// it, until the step's time runs out.
// TODO: the original loads floats[0] before storing the new angle (scheduling; wrap_angle or reading floats[0] first do not help).
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

// allocate_new_laser(LASER_LINE, params) as LTCG inlined it into the wall
// bounce.
static __forceinline void allocate_line_laser_inline(void *params)
{
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
        laser->initialize(params);
    }
}

// The wall bounce et_ex step (ex_flags 0x40): once the tip leaves the
// playfield through a wall enabled in ex_state[4].ints[2] (1 top, 2 bottom,
// 4 left, 8 right), a mirrored laser starts where the laser crosses that
// wall, with ex_state[4].floats[0] as its speed (none with bit 0x10), and
// the step ends. 1 if the laser bounced.
// FUNCTION: TH16 0x432620
i32 LaserLineInf::method_50()
{
    Float3 tip;
    laser_sincosmul(&tip, angle, unk_70);
    tip += position;
    tip.z = 0.0f;
    if (tip.x + 0.0f <= -192.0f || tip.x - 0.0f >= 192.0f || tip.y + 0.0f <= 0.0f || tip.y - 0.0f >= 448.0f)
    {
        i32 bounced = 0;
        if ((ex_state[4].ints[2] & 1) && tip.y < 0.0f)
        {
            if (!(ex_state[4].ints[2] & 0x10))
            {
                collision_segment_intersection(&inner.start_pos.x, &inner.start_pos.y, -256.0f, 0.0f, 256.0f, 0.0f,
                                               tip.x, tip.y, position.x, position.y);
                inner.start_pos.z = 0.0f;
                inner.ang_aim = -angle;
                inner.speed = ex_state[4].floats[0];
                inner.distance = 0.0f;
                allocate_line_laser_inline(&inner);
            }
            bounced = 1;
        }
        if ((ex_state[4].ints[2] & 2) && tip.y > 448.0f)
        {
            if (!(ex_state[4].ints[2] & 0x10))
            {
                collision_segment_intersection(&inner.start_pos.x, &inner.start_pos.y, -256.0f, 448.0f, 256.0f,
                                               448.0f, tip.x, tip.y, position.x, position.y);
                inner.start_pos.z = 0.0f;
                inner.ang_aim = -angle;
                inner.speed = ex_state[4].floats[0];
                inner.distance = 0.0f;
                allocate_line_laser_inline(&inner);
            }
            bounced = 1;
        }
        if ((ex_state[4].ints[2] & 4) && tip.x < -192.0f)
        {
            if (!(ex_state[4].ints[2] & 0x10))
            {
                collision_segment_intersection(&inner.start_pos.x, &inner.start_pos.y, -192.0f, -192.0f, -192.0f,
                                               640.0f, tip.x, tip.y, position.x, position.y);
                inner.start_pos.z = 0.0f;
                inner.ang_aim = normalize_angle(-angle - ZUN_PI);
                inner.speed = ex_state[4].floats[0];
                inner.distance = 0.0f;
                allocate_line_laser_inline(&inner);
            }
            bounced = 1;
        }
        if ((ex_state[4].ints[2] & 8) && tip.x > 192.0f)
        {
            if (!(ex_state[4].ints[2] & 0x10))
            {
                collision_segment_intersection(&inner.start_pos.x, &inner.start_pos.y, 192.0f, -192.0f, 192.0f,
                                               640.0f, tip.x, tip.y, position.x, position.y);
                inner.start_pos.z = 0.0f;
                inner.ang_aim = normalize_angle(-angle - ZUN_PI);
                inner.speed = ex_state[4].floats[0];
                inner.distance = 0.0f;
                allocate_line_laser_inline(&inner);
            }
            bounced = 1;
        }
        if (bounced)
        {
            ex_flags &= ~0x40;
            if (inner.shot_transform_sfx >= 0)
            {
                g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
            }
            return 1;
        }
    }
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
// TODO: the original copies step.x and adds position.x from memory (ours loads position.x first; swapping the operands or step += step does not help) and stores step.z = 0 late from a second zero register.
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
                        allocate_line_laser_inline(&params);
                    }
                } while (j < i);
            }
        }
    }
    return count;
}

// collision_test_circle_rect as LTCG inlined it into the method_1c
// variants.
static __forceinline i32 test_circle_rect_inline(f32 rect_x, f32 rect_y, f32 w, f32 h, f32 angle, f32 circle_x,
                                                 f32 circle_y, f32 radius)
{
    circle_x -= rect_x;
    circle_y -= rect_y;
    angle = -angle;
    f32 s = zun_sinf(angle);
    f32 c = zun_cosf(angle);
    f32 x = circle_x * c - circle_y * s;
    f32 y = circle_x * s + circle_y * c;
    f32 half_w = w * 0.5f;
    f32 abs_x = fabsf(x);
    if (half_w + radius >= abs_x && h * 0.5f >= fabsf(y))
    {
        return 1;
    }
    if (half_w >= abs_x && h * 0.5f + radius >= fabsf(y))
    {
        return 1;
    }
    // Then the corners.
    f32 half_h = h * 0.5f;
    f32 radius_sq = radius * radius;
    if (radius_sq > (x - half_w) * (x - half_w) + (y - half_h) * (y - half_h))
    {
        return 1;
    }
    if (radius_sq > (x + half_w) * (x + half_w) + (y - half_h) * (y - half_h))
    {
        return 1;
    }
    if (radius_sq > (x - half_w) * (x - half_w) + (y + half_h) * (y + half_h))
    {
        return 1;
    }
    if (radius_sq > (x + half_w) * (x + half_w) + (y + half_h) * (y + half_h))
    {
        return 1;
    }
    return 0;
}

// The first boss, as the method_1c variants look it up (inlined
// find_enemy_by_id).
static __forceinline EnemyInf *laser_boss()
{
    return g_EnemyManager->find_enemy_by_id(g_EnemyManager->inner.boss_ids[0]);
}

// The size of the sprite of the boss's first VM.
static __forceinline AnmLoadedSprite *laser_boss_sprite()
{
    AnmVm *vm = get_vm_or_clear(laser_boss()->enemy.anm_ids[0]);
    return &g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
}

static_assert(offsetof(EnemyInf, enemy.anm_ids) == 0x1330, "EnemyInf::enemy.anm_ids");

// Never called. LaserInfiniteInf::method_1c for a straight laser: the boss
// is only tested when it exists, and the damage per point also depends on
// the laser's length, as in LaserCurveInf::method_1c.
// FUNCTION: TH16 0x434010
i32 LaserLineInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    Float3 *pos = (Float3 *)a;
    Float3 *size = (Float3 *)b;
    f32 rect_angle = *(f32 *)&c;
    i32 *boss_hit = (i32 *)f;
    if (e != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    f32 dist = 8.0f;
    i32 count = 0;
    f32 dx = position.x - pos->x;
    f32 dy = position.y - pos->y;
    f32 s = zun_sinf(rect_angle);
    f32 cs = zun_cosf(rect_angle);
    f32 rx = dx * cs - dy * s;
    f32 ry = dx * s + dy * cs;
    Float3 local_step;
    laser_sincosmul(&local_step, wrap_angle(angle + rect_angle), 8.0f);
    f32 local_x = local_step.x + rx;
    f32 local_y = local_step.y + ry;
    local_step.z = 0.0f;
    f32 half_w = size->x * 0.5f;
    f32 half_h = size->y * 0.5f;
    local_step.x += local_step.x;
    local_step.y += local_step.y;
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    f32 world_x = position.x + step.x;
    f32 world_y = position.y + step.y;
    step.x += step.x;
    step.y += step.y;
    step.z = 0.0f;
    u8 hit[0x100];
    u8 *h = hit;
    for (; dist + 8.0f <= unk_70; dist += 16.0f, h++)
    {
        if (*boss_hit == 0 && laser_boss() != NULL)
        {
            if (test_circle_rect_inline(laser_boss()->enemy.final_pos.pos.x, laser_boss()->enemy.final_pos.pos.y,
                                        laser_boss_sprite()->sprite_width * 0.75f,
                                        laser_boss_sprite()->sprite_height * 0.75f, 0.0f, world_x, world_y, 8.0f))
            {
                *boss_hit = 1;
            }
        }
        if (!(-half_w > local_x || local_x > half_w || -half_h > local_y || local_y > half_h))
        {
            count++;
            *h = 1;
            i32 damage = 15;
            if (length >= 12.0f)
            {
                damage = 45;
            }
            else if (length >= 4.0f && 12.0f > length)
            {
                damage = (i32)(((length - 4.0f) / 8.0f * 2.0f + 1.0f) * 15.0f);
            }
            if (width >= 96.0f)
            {
                damage = (i32)(damage + 3.0f + 1.0f);
            }
            else if (width >= 16.0f && 96.0f > width)
            {
                damage = (i32)((width - 16.0f) / 80.0f * 3.0f + damage + 1.0f);
            }
            g_LaserManager->unk_608 += damage;
        }
        world_y += step.y;
        world_x += step.x;
        local_x += local_step.x;
        local_y += local_step.y;
    }
    return count;
}

// Never called. Walks the laser in steps of 16 units: while *boss_hit is
// clear, sets it once a point is within 8 units of the boss's sprite (at
// three quarters size). Counts the points inside the rectangle at pos (size,
// turned by rect_angle) and adds a damage value by laser width for each to
// g_LaserManager->unk_608. Its own points move only 8 units per step in
// the rectangle's frame. The parameters are pos, size, rect_angle, unused,
// e (skip while countdown_5c8 runs) and boss_hit.
// TODO: register allocation differs (the original keeps this in edi, size in esi); it multiplies the sprite sizes before zun_sinf and squares each corner distance again, as in collision_test_circle_rect.
// FUNCTION: TH16 0x436010
i32 LaserInfiniteInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    Float3 *pos = (Float3 *)a;
    Float3 *size = (Float3 *)b;
    f32 rect_angle = *(f32 *)&c;
    i32 *boss_hit = (i32 *)f;
    if (e != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    f32 dist = 8.0f;
    i32 count = 0;
    f32 dx = position.x - pos->x;
    f32 dy = position.y - pos->y;
    f32 s = zun_sinf(rect_angle);
    f32 cs = zun_cosf(rect_angle);
    f32 rx = dx * cs - dy * s;
    f32 ry = dx * s + dy * cs;
    Float3 local_step;
    laser_sincosmul(&local_step, wrap_angle(angle + rect_angle), 8.0f);
    f32 local_x = local_step.x + rx;
    f32 local_y = local_step.y + ry;
    local_step.z = 0.0f;
    f32 half_w = size->x * 0.5f;
    f32 half_h = size->y * 0.5f;
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    f32 world_x = position.x + step.x;
    f32 world_y = position.y + step.y;
    step.x += step.x;
    step.y += step.y;
    step.z = 0.0f;
    u8 hit[0x100];
    u8 *h = hit;
    for (; dist + 8.0f <= unk_70; dist += 16.0f, h++)
    {
        if (*boss_hit == 0)
        {
            if (test_circle_rect_inline(laser_boss()->enemy.final_pos.pos.x, laser_boss()->enemy.final_pos.pos.y,
                                        laser_boss_sprite()->sprite_width * 0.75f,
                                        laser_boss_sprite()->sprite_height * 0.75f, 0.0f, world_x, world_y, 8.0f))
            {
                *boss_hit = 1;
            }
        }
        if (!(-half_w > local_x || local_x > half_w || -half_h > local_y || local_y > half_h))
        {
            count++;
            *h = 1;
            i32 damage = 18;
            if (width >= 96.0f)
            {
                damage = (i32)(damage + 3.0f + 1.0f);
            }
            else if (width >= 16.0f && 96.0f > width)
            {
                damage = (i32)((width - 16.0f) / 80.0f * 3.0f + damage + 1.0f);
            }
            g_LaserManager->unk_608 += damage;
        }
        local_x += local_step.x;
        world_x += step.x;
        local_y += local_step.y;
        world_y += step.y;
    }
    return count;
}

// Never called. LaserInfiniteInf::method_1c for the segments of a curvy
// laser: sets *boss_hit once a segment is within 8 units of the boss's
// sprite, and counts the segments inside the rectangle, adding a damage
// value by laser length and width for each to g_LaserManager->unk_608.
// TODO: as LaserInfiniteInf::method_1c; ours also keeps g_EnemyManager in edi across the loop where the original reloads it.
// FUNCTION: TH16 0x439d60
i32 LaserCurveInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    Float3 *center = (Float3 *)a;
    Float3 *size = (Float3 *)b;
    f32 rect_angle = *(f32 *)&c;
    i32 *boss_hit = (i32 *)f;
    if (e != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    i32 count = 0;
    LaserCurveSegment *segment = (LaserCurveSegment *)unk_1524;
    for (i32 i = 0; i < inner.segment_count; i++, segment++)
    {
        Float3 seg_pos = segment->pos;
        if (*boss_hit == 0)
        {
            if (test_circle_rect_inline(laser_boss()->enemy.final_pos.pos.x, laser_boss()->enemy.final_pos.pos.y,
                                        laser_boss_sprite()->sprite_width * 0.75f,
                                        laser_boss_sprite()->sprite_height * 0.75f, 0.0f, seg_pos.x, seg_pos.y, 8.0f))
            {
                *boss_hit = 1;
            }
        }
        f32 half_w = size->x;
        f32 half_h = size->y;
        f32 dx = seg_pos.x - center->x;
        f32 dy = seg_pos.y - center->y;
        if (rect_angle != 0.0f)
        {
            f32 neg_angle = -rect_angle;
            f32 s = zun_sinf(neg_angle);
            f32 cs = zun_cosf(neg_angle);
            f32 rx = dx * cs - dy * s;
            dy = dy * cs + dx * s;
            dx = rx;
        }
        if (half_w * 0.5f >= (f32)fabs(dx) && half_h * 0.5f >= (f32)fabs(dy))
        {
            count++;
            i32 damage = 15;
            if (length >= 12.0f)
            {
                damage = 45;
            }
            else if (length >= 4.0f && 12.0f > length)
            {
                damage = (i32)(((length - 4.0f) / 8.0f * 2.0f + 1.0f) * 15.0f);
            }
            if (width >= 96.0f)
            {
                damage = (i32)(damage + 3.0f + 1.0f);
            }
            else if (width >= 16.0f && 96.0f > width)
            {
                damage = (i32)((width - 16.0f) / 80.0f * 3.0f + damage + 1.0f);
            }
            g_LaserManager->unk_608 += damage;
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
                allocate_line_laser_inline(&params);
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
// FUNCTION: TH16 0x433510
i32 LaserLineInf::check_graze_or_kill(i32 graze_only)
{
    if (unk_70 > 16.0f && width > 3.0f)
    {
        Float3 start;
        if (!(inner.flags & 2))
        {
            laser_sincosmul(&start, angle, unk_70 / 10.0f);
            D3DXVec3Add(&start, &start, &position);
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
                collision_line_intersection(&x, &y, start.x, start.y, angle, g_Player->inner.pos.x,
                                            g_Player->inner.pos.y, normalize_angle(angle + ZUN_PI / 2));
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
                collision_line_intersection(&x, &y, start.x, start.y, angle, g_Player->inner.pos.x,
                                            g_Player->inner.pos.y, normalize_angle(angle + ZUN_PI / 2));
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
// TODO: after the hit test the original reloads 0.5 and dist at the loop join, ours at the start of the result == 2 test (nested if or continue forms do not help).
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
    timer_2c.tick_split();
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
                        allocate_line_laser_inline(&params);
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
            allocate_line_laser_inline(&params);
        }
    }
    return count;
}

// Sets the laser up from its parameters: the body and its origin VM, the
// shot sound, and the start offset along the aim.
// TODO: the original realigns the frame (and esp, -8); everything else matches. A dead double in a HARNESS_CALLED AnmVm::run matches it (see README).
// FUNCTION: TH16 0x435050
i32 LaserInfiniteInf::initialize(void *params)
{
    inner = *(LaserInfiniteInner *)params;
    bullet_type = inner.type;
    state = 3;
    kind = LASER_INFINITE;
    bullet_color = inner.color;
    AnmVm *vm = &vm_950;
    vm->wipe();
    vm_950.index_of_sprite_mapping_func = 2;
    vm_950.associated_game_entity = this;
    g_LaserManager->bullet_anm->set_vm_script(vm, g_bullet_types[bullet_type].script);
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    AnmVmFlagsLoFields *fields = (AnmVmFlagsLoFields *)&vm_950.flags_lo;
    fields->anchor_x = 0;
    fields->anchor_y = 2;
    fields->render_mode = 1;
    vm_950.flags_hi = vm_950.flags_hi & ~0x80000 | 0x40000;
    vm = &vm_f4c;
    g_LaserManager->bullet_anm->copy_vm(vm, inner.color + 0x38);
    vm->unk_5b0 = NULL;
    vm->parent = NULL;
    vm->run();
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    ((AnmVmFlagsLoFields *)&vm_f4c.flags_lo)->render_mode = 1;
    vm_f4c.flags_hi = vm_f4c.flags_hi & ~0x80000 | 0x40000;
    if (inner.shot_sfx >= 0)
    {
        g_SoundManager.play_sound_at_position(inner.shot_sfx, 0.0f);
    }
    position = inner.start_pos;
    if (inner.distance != 0.0f)
    {
        Float3 offset;
        laser_sincosmul(&offset, inner.ang_aim, inner.distance);
        position.x += offset.x;
        position.y += offset.y;
    }
    unk_70 = inner.laser_new_arg_1;
    length = inner.speed;
    angle = inner.ang_aim;
    ex_index = *(i32 *)inner.unk_50;
    width = 2.0f;
    id = inner.laser_st_on_arg_1;
    timer_2c.reset();
    unk_94c = 0;
    return 0;
}

// Sets a beam up from its parameters.
// TODO: in the inlined wipe the original schedules the flags_hi and/or one store later (an instruction scheduling difference only).
// FUNCTION: TH16 0x43a860
i32 LaserBeamInf::initialize(void *params)
{
    inner = *(LaserBeamInner *)params;
    position = inner.start_pos;
    unk_70 = inner.length;
    angle = inner.ang_aim;
    bullet_color = inner.color;
    state = 3;
    kind = LASER_BEAM;
    id = inner.id;
    for (i32 i = 0; i < 0x200; i++)
    {
        unk_f28[i] = unk_70;
    }
    width = 1.0f;
    unk_f24 = 0;
    vm_928.wipe();
    AnmVm *vm = &vm_928;
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    return 0;
}

// Sets a curvy laser up from its parameters: the body and origin VMs, the
// segment buffers (every segment at the start), the start offset along the
// aim, and the node list: a copy of the source laser's when a bomb split
// this one off (et_ex then skipped), else one straight node. Then places
// the segments for the starting time.
// TODO: the original adds the offset onto the loaded position (operand order), reloads angle for the straight node's velocity, and loads unk_1524 before scaling i.
// FUNCTION: TH16 0x4370a0
i32 LaserCurveInf::initialize(void *params)
{
    inner = *(LaserCurveInner *)params;
    bullet_type = inner.type;
    state = 2;
    kind = LASER_CURVE;
    bullet_color = inner.color;
    AnmVm *vm = &vm_92c;
    vm->wipe();
    if (bullet_type == 1)
    {
        g_LaserManager->bullet_anm->set_vm_script(vm, 0x142);
    }
    else
    {
        vm_92c.index_of_sprite_mapping_func = 3;
        vm_92c.associated_game_entity = this;
        g_LaserManager->bullet_anm->set_vm_script(vm, bullet_type + 0x8e);
    }
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    AnmVmFlagsLoFields *fields = (AnmVmFlagsLoFields *)&vm_92c.flags_lo;
    fields->anchor_x = 0;
    fields->anchor_y = 2;
    fields->render_mode = 1;
    vm_92c.flags_hi = vm_92c.flags_hi & ~0x80000 | 0x40000;
    vm = &vm_f28;
    g_LaserManager->bullet_anm->copy_vm(vm, inner.color + 0x38);
    vm->unk_5b0 = NULL;
    vm->parent = NULL;
    vm->run();
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    ((AnmVmFlagsLoFields *)&vm_f28.flags_lo)->render_mode = 1;
    vm_f28.flags_hi = vm_f28.flags_hi & ~0x80000 | 0x40000;
    unk_1528 = malloc(inner.segment_count * 0x38);
    unk_1524 = malloc(inner.segment_count * sizeof(LaserCurveSegment));
    position = inner.start_pos;
    if (inner.distance != 0.0f)
    {
        Float3 offset;
        laser_sincosmul(&offset, inner.ang_aim, inner.distance);
        position.x += offset.x;
        position.y += offset.y;
        inner.start_pos = position;
        inner.distance = 0.0f;
    }
    width = inner.laser_new_arg_4;
    angle = inner.ang_aim;
    length = inner.speed;
    laser_sincosmul(&unk_60, angle, length);
    unk_60.z = 0.0f;
    for (i32 i = 0; i < inner.segment_count; i++)
    {
        ((LaserCurveSegment *)unk_1524)[i].pos = position;
        *(Float3 *)((LaserCurveSegment *)unk_1524)[i].unk_c = g_zero_vec;
        ((LaserCurveSegment *)unk_1524)[i].angle = inner.ang_aim;
        ((LaserCurveSegment *)unk_1524)[i].length = inner.speed;
    }
    timer_40.set_f(inner.source_time);
    if (inner.source_nodes != NULL)
    {
        nodes = *inner.source_nodes;
        LaserCurveNode *dst = &nodes;
        for (LaserCurveNode *src = inner.source_nodes; src != NULL; src = src->next)
        {
            if (src->next != NULL)
            {
                dst->next = new LaserCurveNode;
                *dst->next = *src->next;
                dst = dst->next;
            }
        }
        inner.source_nodes = NULL;
        ex_index = 99;
    }
    else
    {
        nodes.next = NULL;
        nodes.speed = length;
        nodes.angle = wrap_angle(angle);
        laser_sincosmul(&nodes.velocity, angle, 1.0f);
        nodes.start_pos = position;
        nodes.velocity.z = 0.0f;
        nodes.mode = 0;
        nodes.unk_8 = 0.0f;
        nodes.unk_c = 999999.0f;
        ex_index = *(i32 *)inner.unk_34c;
    }
    *(Float3 *)((LaserCurveSegment *)unk_1524)->unk_c = unk_60;
    for (i32 i = 0; i < inner.segment_count; i++)
    {
        LaserCurveSegment *segment = &((LaserCurveSegment *)unk_1524)[i];
        Float3 *prev_pos = &segment[-1].pos;
        f32 *out_length = &segment->length;
        f32 prev_length = segment[-1].length;
        f32 prev_angle = segment[-1].angle;
        f32 *out_angle = &segment->angle;
        f32 t = timer_40.current_f - (f32)i;
        for (LaserCurveNode *node = &nodes; node != NULL; node = node->next)
        {
            if (t >= node->unk_8 && node->unk_c > t)
            {
                if (i == 0)
                {
                    node->get_state(&segment->pos, out_length, out_angle, t);
                }
                else
                {
                    node->step_back(&segment->pos, out_length, out_angle, prev_pos, prev_length, prev_angle, t);
                }
                break;
            }
        }
    }
    timer_5a0.set_inline(30);
    if (inner.shot_sfx >= 0)
    {
        g_SoundManager.play_sound_at_position(inner.shot_sfx, 0.0f);
    }
    timer_2c.reset();
    return 0;
}

static_assert(offsetof(AnmVm, pos) == 0x2c, "AnmVm::pos");
static_assert(offsetof(AnmVm, uv_quad_of_sprite) == 0x3a8, "AnmVm::uv_quad_of_sprite");

// The angle halfway from cur to prev, going the short way round.
static __forceinline f32 laser_mid_angle(f32 cur, f32 prev)
{
    f32 d;
    if (prev - cur > ZUN_PI)
    {
        d = prev - (cur + ZUN_2PI);
    }
    else if (cur - prev > ZUN_PI)
    {
        d = prev - (cur - ZUN_2PI);
    }
    else
    {
        d = prev - cur;
    }
    d = wrap_angle(d);
    d = wrap_angle(d * 0.5f);
    return wrap_angle(d + cur);
}

// Draws the body as a triangle strip: two vertices per segment, half the
// laser's width to each side across the segment's direction (averaged with
// the previous segment's), with u running from 0 to 1 along the laser. The
// origin VM sits on the last segment until the whole laser is out.
// TODO: the original loads the segment z before adding the vertex z (operand order; += gets z right but y wrong, D3DXVec3Add or field-wise forms get z wrong).
// FUNCTION: TH16 0x438750
i32 LaserCurveInf::on_draw()
{
    f32 u = 0.0f;
    RenderVertex144 *vertex = (RenderVertex144 *)unk_1528;
    LaserCurveSegment *segment = (LaserCurveSegment *)unk_1524;
    for (i32 i = 0; i < inner.segment_count; i++, segment++, vertex++)
    {
        vertex->pos.w = 1.0f;
        vertex->diffuse = 0xffffffff;
        vertex->uv.x = u;
        vertex->uv.y = vm_92c.uv_quad_of_sprite[0].y;
        f32 a;
        if (i == 0)
        {
            a = wrap_angle(segment->angle + ZUN_PI / 2);
        }
        else
        {
            f32 cur = wrap_angle(segment->angle + ZUN_PI / 2);
            a = laser_mid_angle(cur, wrap_angle(segment[-1].angle + ZUN_PI / 2));
        }
        laser_sincosmul((Float3 *)&vertex->pos, a, inner.laser_new_arg_4 * 0.5f);
        D3DXVec3Add((Float3 *)&vertex->pos, &segment->pos, (Float3 *)&vertex->pos);
        vertex->pos.x += (f32)g_game_2d_origin_x;
        vertex->pos.y += (f32)g_early_arcade_offset_y;
        vertex->pos.z = 0.0f;
        vertex++;
        vertex->pos.w = 1.0f;
        vertex->diffuse = 0xffffffff;
        vertex->uv.x = u;
        vertex->uv.y = vm_92c.uv_quad_of_sprite[2].y;
        if (i == 0)
        {
            a = wrap_angle(segment->angle - ZUN_PI / 2);
        }
        else
        {
            f32 cur = wrap_angle(segment->angle - ZUN_PI / 2);
            a = laser_mid_angle(cur, wrap_angle(segment[-1].angle - ZUN_PI / 2));
        }
        laser_sincosmul((Float3 *)&vertex->pos, a, inner.laser_new_arg_4 * 0.5f);
        D3DXVec3Add((Float3 *)&vertex->pos, &segment->pos, (Float3 *)&vertex->pos);
        vertex->pos.x += (f32)g_game_2d_origin_x;
        vertex->pos.y += (f32)g_early_arcade_offset_y;
        vertex->pos.z = 0.0f;
        u += 1.0f / (f32)(inner.segment_count - 1);
    }
    g_AnmManager->draw_vm__mode_9(&vm_92c, (RenderVertex144 *)unk_1528, inner.segment_count * 2);
    if (inner.segment_count >= timer_40.current)
    {
        vm_f28.pos = ((LaserCurveSegment *)unk_1524)[inner.segment_count - 1].pos;
        g_AnmManager->draw_vm(&vm_f28);
    }
    return 0;
}

// Runs the laser's pending et_ex instructions, as LaserLineInf::run_ex
// does, except that 4 and 8 add nodes to the curve: one moving by velocity
// or by speed and angle deltas from the given time, followed by a straight
// one after a duration (a) unless that is negative. An instruction already
// running stops the list.
// TODO: the original keeps ex as a pointer (this + 0x600 + index * 0x2c) where ours addresses through this plus the scaled index; case 4 stores start_pos.z after loading the angle, and (f32)b goes to xmm1.
// FUNCTION: TH16 0x438cb0
void LaserCurveInf::run_ex()
{
    while (ex_index < 0x12)
    {
        BulletEx *ex = &inner.ex[ex_index];
        if (ex->type == 0)
        {
            return;
        }
        if (ex->slot == 0 && ex_flags != 0)
        {
            return;
        }
        if (ex->type & ex_flags)
        {
            return;
        }
        switch ((u32)ex->type)
        {
        case 1:
            ex_flags |= 1;
            ex_state[0].timer.set_value(0);
            ex_state[0].floats[7] = 0.0f;
            break;
        case 4:
        {
            LaserCurveNode *node = append_node((f32)ex->b);
            node->mode = 1;
            node->prev->get_state(&node->start_pos, &node->speed, &node->angle, node->prev->unk_c);
            node->start_pos.z = 0.0f;
            laser_sincosmul(&node->velocity, node->angle, 1.0f);
            node->velocity.z = 0.0f;
            node->speed_delta = ex->r;
            node->angle_delta = ex->s;
            if (ex->a >= 0)
            {
                node->unk_c = (f32)ex->a + (f32)ex->b;
                node = append_node(node->unk_c);
                node->mode = 0;
                node->prev->get_state(&node->start_pos, &node->speed, &node->angle, node->prev->unk_c);
                node->start_pos.z = 0.0f;
                laser_sincosmul(&node->velocity, node->angle, 1.0f);
                node->velocity.z = 0.0f;
                node->unk_c = 999999.0f;
            }
            else
            {
                node->unk_c = 999999.0f;
            }
            break;
        }
        case 8:
        {
            LaserCurveNode *node = append_node((f32)ex->b);
            node->mode = 2;
            node->prev->get_state(&node->start_pos, &node->speed, &node->angle, node->prev->unk_c);
            node->start_pos.z = 0.0f;
            laser_sincosmul(&node->velocity, node->angle, 1.0f);
            node->velocity.z = 0.0f;
            node->speed_delta = ex->r;
            node->angle_delta = ex->s;
            if (ex->a >= 0)
            {
                node->unk_c = (f32)ex->a + (f32)ex->b;
                node = append_node(node->unk_c);
                node->mode = 0;
                node->prev->get_state(&node->start_pos, &node->speed, &node->angle, node->prev->unk_c);
                node->start_pos.z = 0.0f;
                laser_sincosmul(&node->velocity, node->angle, 1.0f);
                node->velocity.z = 0.0f;
                node->unk_c = 999999.0f;
            }
            else
            {
                node->unk_c = 999999.0f;
            }
            break;
        }
        case 0x10:
            ex_flags |= ex->type;
            ex_state[3].floats[1] = ex->r;
            ex_state[3].floats[0] = ex->s > -999.0f ? ex->s : length;
            ex_state[3].timer.set_value(0);
            ex_state[3].ints[0] = ex->a;
            ex_state[3].ints[1] = ex->b;
            ex_state[3].ints[2] = 0;
            ex_state[3].ints[3] = ex->c;
            break;
        case 0x40:
            if (ex->a > 0)
            {
                ex_flags |= ex->type;
                if (ex->r >= 0.0f)
                {
                    ex_state[4].floats[0] = ex->r;
                }
                else
                {
                    ex_state[4].floats[0] = length;
                }
                ex->a--;
                ex_state[4].ints[1] = ex->a;
                ex_state[4].ints[0] = 0;
                ex_state[4].ints[2] = ex->b;
            }
            break;
        case 0x80:
            countdown_5c8 = ex->a;
            break;
        case 0x100:
            ex_flags |= ex->type;
            ex_state[11].timer.set_inline(ex->a);
            ex_state[11].ints[0] = ex->b;
            break;
        case 0x200:
        {
            AnmVm *vm = &vm_92c;
            g_BulletManager->bullet_anm->copy_vm(vm, g_bullet_types[ex->a].script + ex->b);
            vm->unk_5b0 = NULL;
            vm->parent = NULL;
            vm->run();
            break;
        }
        case 0x400:
            state = 3;
            break;
        case 0x800:
            g_SoundManager.play_sound_at_position(ex->a, position.x);
            break;
        case 0x1000:
            ex_flags |= ex->type;
            ex_state[6].timer.set_value(ex->a);
            break;
        case 0x2000:
        {
            EnemyBulletShooter shooter;
            laser_sincosmul(&shooter.pos, angle, unk_70);
            u32 a = ex->a;
            shooter.pos.x += position.x;
            shooter.pos.z = 0.0f;
            *(u16 *)&shooter.aim_type = (a >> 24) & 0x7f;
            shooter.type = (a >> 16) & 0xff;
            shooter.pos.y += position.y;
            shooter.color = (a >> 8) & 0xff;
            shooter.spd1 = ex->r;
            shooter.spd2 = ex->s;
            shooter.start_transform = a & 0xff;
            shooter.count = ex->b;
            ex_index++;
            shooter.layers = ex[1].a;
            shooter.ang_aim = ex[1].r;
            shooter.sfx_flags = ex[1].b;
            shooter.ang_bullet_dist = ex[1].s;
            memcpy(shooter.ex, inner.ex, sizeof(inner.ex));
            g_BulletManager->shoot_bullets(&shooter);
            ex_index++;
            if ((i32)a < 0)
            {
                cancel(0, 0);
                break;
            }
            continue;
        }
        case 0x8000:
            id = ex->a;
            ex_index++;
            continue;
        case 0x10000:
            ex_index = ex->a;
            continue;
        case 0x100000:
            if (ex->a != 0)
            {
                vm_92c.flags_lo = vm_92c.flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
            }
            else
            {
                vm_92c.flags_lo &= ~ANM_VM_BLEND_MODE_MASK;
            }
            break;
        case 0x10000000:
            ((LaserDataFlagBits *)(&next + 1))->segments_frozen = ex->a;
            break;
        case 0x80000000:
            if (ex->a > 0)
            {
                ex_flags |= ex->type;
                ex_state[5].timer.set_value(ex->a);
                break;
            }
            ex_index++;
            continue;
        }
        ex_index++;
    }
}

// TODO: register allocation and the order of the vector temporaries differ (the original builds them with unpcklps).
// FUNCTION: TH16 0x438370
void LaserCurveNode::step_back(Float3 *out_pos, f32 *out_speed, f32 *out_angle, Float3 *pos, f32 speed, f32 angle,
                               f32 t)
{
    switch (mode)
    {
    case 0:
        *out_pos = *pos - velocity * this->speed;
        *out_speed = this->speed;
        *out_angle = this->angle;
        break;
    case 1:
        if (-990.0f > angle_delta)
        {
            f32 dt = speed - speed_delta;
            *out_pos = *pos - velocity * dt;
            *out_speed = this->speed - speed_delta;
            *out_angle = angle;
        }
        else
        {
            Float3 a;
            Float3 b;
            a.z = 0.0f;
            b.z = 0.0f;
            laser_sincosmul(&a, angle, -speed);
            laser_sincosmul(&b, angle_delta, -speed_delta);
            Float3 sum = b + a;
            *out_pos = *pos + sum;
            *out_speed = (f32)sqrt(sum.x * sum.x + sum.y * sum.y);
            *out_angle = atan2(sum.y, sum.x);
        }
        break;
    case 2:
    {
        Float3 d;
        d.z = 0.0f;
        laser_sincosmul(&d, angle, speed);
        f32 whole = (f32)floor(t);
        *out_pos = *pos - d * (t - whole);
        *out_speed = speed - speed_delta;
        i32 i = 0;
        f32 a = angle - angle_delta;
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
        *out_angle = a;
        laser_sincosmul(&d, a, *out_speed);
        *out_pos = *out_pos - d * (1.0f - t + whole);
        break;
    }
    }
}

// TODO: register allocation differs (the original keeps the stepped position in xmm registers and stack shadows; the frame is aligned to 64).
// FUNCTION: TH16 0x437ee0
void LaserCurveNode::get_state(Float3 *out_pos, f32 *out_speed, f32 *out_angle, f32 time)
{
    time -= unk_8;
    switch (mode)
    {
    case 0:
        *out_pos = start_pos + velocity * time * speed;
        *out_speed = speed;
        *out_angle = angle;
        break;
    case 1:
        if (-990.0f > angle_delta)
        {
            *out_pos = start_pos + velocity * (speed + speed + speed_delta * time) * (time + 1.0f) * 0.5f;
            *out_speed = speed_delta * time + speed;
            *out_angle = angle;
        }
        else
        {
            Float3 start = start_pos;
            Float3 a;
            Float3 b;
            a.z = 0.0f;
            b.z = 0.0f;
            laser_sincosmul(&a, angle, speed);
            laser_sincosmul(&b, angle_delta, speed_delta);
            Float3 sum = b + a;
            *out_pos = start + sum * time;
            *out_speed = (f32)sqrt(sum.x * sum.x + sum.y * sum.y);
            *out_angle = atan2(sum.y, sum.x);
        }
        break;
    case 2:
    {
        Float3 pos = start_pos;
        f32 a = angle;
        f32 s = speed;
        Float3 d;
        d.z = 0.0f;
        for (i32 n = (i32)time; n > 0; n--)
        {
            laser_sincosmul(&d, a, s);
            i32 i = 0;
            a += angle_delta;
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
            pos.x += d.x;
            s += speed_delta;
            pos.y += d.y;
            pos.z += d.z;
        }
        laser_sincosmul(&d, a, s);
        *out_pos = pos + d * (time - (f32)floor(time));
        *out_speed = s;
        *out_angle = a;
        break;
    }
    }
}

// One frame: the et_ex steps until none asks to run again, growth (or, at
// full length, moving and shrinking to laser_new_arg_3), leaving the screen
// once the two delay timers ran out, then the graze check and the VMs.
// Nonzero once the laser is done.
// TODO: the original realigns the frame, keeps the * 1.0f of the inlined timer decrement, and loads g_game_speed once for the three position components.
// FUNCTION: TH16 0x432f40
i32 LaserLineInf::on_tick()
{
    i32 again;
    do
    {
        run_ex();
        if (ex_flags == 0)
        {
            break;
        }
        again = 0;
        if (ex_flags & 1)
        {
            again = method_38();
        }
        if (ex_flags & 4)
        {
            again += method_3c();
        }
        if (ex_flags & 8)
        {
            again += method_40();
        }
        if (ex_flags & 0x10)
        {
            switch (ex_state[3].ints[3])
            {
            case 0:
                again += method_44();
                break;
            case 1:
                again += method_4c();
                break;
            case 4:
                again += method_48();
                break;
            }
        }
        if (ex_flags & 0x40)
        {
            again += method_50();
        }
        if (ex_flags & 0x1000)
        {
            again += method_54();
        }
        if ((i32)ex_flags < 0)
        {
            if (ex_state[5].timer.current <= 0)
            {
                ex_flags ^= 0x80000000;
                again++;
            }
            else
            {
                ex_state[5].timer.decrement(1.0f);
            }
        }
        if (countdown_5c8 != 0)
        {
            countdown_5c8--;
        }
    } while (again != 0);
    f32 step = length * g_game_speed;
    if (unk_70 < inner.laser_new_arg_2)
    {
        unk_70 = step + unk_70;
        if (unk_70 > inner.laser_new_arg_2)
        {
            unk_70 = inner.laser_new_arg_2;
        }
    }
    else
    {
        unk_7c = step + unk_7c;
        position.x = unk_60.x * g_game_speed + position.x;
        position.y = position.y + unk_60.y * g_game_speed;
        position.z = position.z + unk_60.z * g_game_speed;
        if (inner.laser_new_arg_3 > 0.0f && unk_70 + unk_7c > inner.laser_new_arg_3)
        {
            unk_70 = inner.laser_new_arg_3 - unk_7c;
            inner.laser_new_arg_2 = unk_70;
            if (0.0f >= unk_70)
            {
                return 1;
            }
        }
    }
    if (timer_5a0.current > 0 || timer_5b4.current > 0)
    {
        if (timer_5a0.current > 0)
        {
            timer_5a0.decrement(1.0f);
        }
        if (timer_5b4.current > 0)
        {
            timer_5b4.decrement(1.0f);
        }
    }
    else
    {
        Float3 tip;
        laser_sincosmul(&tip, angle, unk_70);
        f32 tip_x = position.x + tip.x;
        f32 tip_y = position.y + tip.y;
        if ((position.x + width <= -192.0f || position.x - width >= 192.0f || position.y + width <= 0.0f ||
             position.y - width >= 448.0f) &&
            (tip_x + width <= -192.0f || tip_x - width >= 192.0f || tip_y + width <= 0.0f || tip_y - width >= 448.0f))
        {
            return 1;
        }
    }
    check_graze_or_kill(0);
    AnmVm *vm = &vm_92c;
    vm->flags_lo |= ANM_VM_SCALE_CHANGED;
    vm->scale.x = width / g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].sprite_width;
    vm->flags_lo |= ANM_VM_SCALE_CHANGED;
    vm->scale.y = unk_70 / g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].sprite_height;
    vm->run();
    if (unk_7c == 0.0f)
    {
        vm_f28.run();
    }
    vm_1524.run();
    timer.tick();
    return 0;
}

// One frame: the et_ex steps, then each segment follows the node list to
// its place at timer_40 minus its index (segments not out yet stay at the
// start), leaving the screen once every segment is off it.
// TODO: the original walks the segments with a pointer biased by -8 and keeps the constants 192 and 448 in swapped registers; it also keeps the * 1.0f of the inlined timer decrement.
// FUNCTION: TH16 0x4377d0
i32 LaserCurveInf::on_tick()
{
    i32 again;
    do
    {
        run_ex();
        if (ex_flags == 0)
        {
            break;
        }
        again = 0;
        if (ex_flags & 1)
        {
            again = method_38();
        }
        if (ex_flags & 4)
        {
            again += method_3c();
        }
        if (ex_flags & 8)
        {
            again += method_40();
        }
        if (ex_flags & 0x10)
        {
            switch (ex_state[3].ints[3])
            {
            case 0:
                again += method_44();
                break;
            case 1:
                again += method_4c();
                break;
            case 4:
                again += method_48();
                break;
            }
        }
        if (ex_flags & 0x40)
        {
            again += method_50();
        }
        if (ex_flags & 0x1000)
        {
            again += method_54();
        }
        if (ex_flags & 0x100)
        {
            again += method_60();
        }
        if ((i32)ex_flags < 0)
        {
            if (ex_state[5].timer.current <= 0)
            {
                ex_flags ^= 0x80000000;
                again++;
            }
            else
            {
                ex_state[5].timer.decrement(1.0f);
            }
        }
        if (countdown_5c8 != 0)
        {
            countdown_5c8--;
        }
    } while (again != 0);
    LaserCurveSegment *segment = (LaserCurveSegment *)unk_1524;
    if (!(flags_rest & 1))
    {
        i32 placed = 0;
        for (i32 i = 0; i < inner.segment_count; i++, segment++)
        {
            f32 t = timer_40.current_f - (f32)i;
            if (t >= 0.0f)
            {
                f32 prev_length = segment[-1].length;
                f32 prev_angle = segment[-1].angle;
                f32 *out_length = &segment->length;
                f32 *out_angle = &segment->angle;
                LaserCurveNode *node;
                for (node = &nodes; node != NULL; node = node->next)
                {
                    if (t >= node->unk_8 && node->unk_c > t)
                    {
                        if (!placed)
                        {
                            node->get_state(&segment->pos, out_length, out_angle, t);
                        }
                        else
                        {
                            node->step_back(&segment->pos, out_length, out_angle, &segment[-1].pos, prev_length,
                                            prev_angle, t);
                        }
                        break;
                    }
                }
                placed = 1;
            }
            else
            {
                segment->pos = inner.start_pos;
                *(Float3 *)segment->unk_c = g_zero_vec;
                segment->angle = inner.ang_aim;
                segment->length = inner.speed;
            }
        }
    }
    segment = (LaserCurveSegment *)unk_1524;
    if (timer_5a0.current > 0 || (ex_flags & 0x100))
    {
        timer_5a0.decrement(1.0f);
    }
    else
    {
        for (i32 i = 0; i < inner.segment_count; i++, segment++)
        {
            Float3 head;
            laser_sincosmul(&head, angle, unk_70);
            head += position;
            if (!(segment->pos.x + width <= -192.0f || segment->pos.x - width >= 192.0f ||
                  segment->pos.y + width <= 0.0f || segment->pos.y - width >= 448.0f))
            {
                goto on_screen;
            }
        }
        return 1;
    }
on_screen:
    check_graze_or_kill(0);
    vm_92c.run();
    vm_f28.run();
    timer_40.tick();
    return 0;
}

// Runs the laser's pending et_ex instructions: each one starts an et_ex
// step (ex_flags and its ex_state), or acts at once (sounds, bullets, the
// sprite, blend mode, jumps).
// TODO: the shooter fields after pos are written through raw offsets; register allocation and the case layout differ.
// FUNCTION: TH16 0x431fe0
DECOMP_NOINLINE void LaserLineInf::run_ex()
{
    while (ex_index < 0x12)
    {
        BulletEx *ex = &inner.ex[ex_index];
        if (ex->type == 0)
        {
            return;
        }
        if (ex->slot == 0 && ex_flags != 0)
        {
            return;
        }
        switch ((u32)ex->type)
        {
        case 1:
            ex_flags |= 1;
            ex_state[0].timer.set_value(0);
            ex_state[0].floats[7] = 0.0f;
            break;
        case 4:
            ex_flags |= 4;
            ex_state[1].floats[0] = ex->r;
            ex_state[1].floats[1] =
                -990.0f >= ex->s ? angle : (ex->s >= 990.0f ? g_Player->angle_to_player(&position) : ex->s);
            ex_state[1].timer.set_value(0);
            ex_state[1].ints[0] = ex->a;
            laser_sincosmul((Float3 *)&ex_state[1].floats[5], ex_state[1].floats[1], ex_state[1].floats[0]);
            if (ex_index != 0 && inner.shot_transform_sfx >= 0)
            {
                g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
            }
            break;
        case 8:
            ex_flags |= 8;
            ex_state[2].floats[0] = ex->r;
            ex_state[2].floats[1] = ex->s;
            ex_state[2].timer.set_value(0);
            ex_state[2].ints[0] = ex->a;
            if (ex_index != 0 && inner.shot_transform_sfx >= 0)
            {
                g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
            }
            break;
        case 0x10:
            ex_flags |= ex->type;
            ex_state[3].floats[1] = ex->r;
            ex_state[3].floats[0] = ex->s > -999.0f ? ex->s : length;
            ex_state[3].timer.set_value(0);
            ex_state[3].ints[0] = ex->a;
            ex_state[3].ints[1] = ex->b;
            ex_state[3].ints[2] = 0;
            ex_state[3].ints[3] = ex->c;
            break;
        case 0x40:
            if (ex->a > 0)
            {
                ex_flags |= ex->type;
                if (ex->r >= 0.0f)
                {
                    ex_state[4].floats[0] = ex->r;
                }
                else
                {
                    ex_state[4].floats[0] = length;
                }
                ex->a--;
                ex_state[4].ints[1] = ex->a;
                ex_state[4].ints[0] = 0;
                ex_state[4].ints[2] = ex->b;
            }
            break;
        case 0x80:
            countdown_5c8 = ex->a;
            break;
        case 0x100:
            timer_5b4.set_inline(ex->a);
            ex_index++;
            continue;
        case 0x200:
        {
            AnmVm *vm = &vm_92c;
            g_BulletManager->bullet_anm->copy_vm(vm, g_bullet_types[ex->a].script + ex->b);
            vm->unk_5b0 = NULL;
            vm->parent = NULL;
            vm->run();
            break;
        }
        case 0x400:
            state = 3;
            break;
        case 0x800:
            g_SoundManager.play_sound_at_position(ex->a, position.x);
            ex_index++;
            continue;
        case 0x1000:
            ex_flags |= ex->type;
            ex_state[6].timer.set_inline(ex->a);
            break;
        case 0x2000:
        {
            EnemyBulletShooter shooter;
            laser_sincosmul(&shooter.pos, angle, unk_70);
            u32 a = ex->a;
            shooter.pos.x += position.x;
            shooter.pos.z = 0.0f;
            *(u16 *)&shooter.aim_type = (a >> 24) & 0x7f;
            shooter.type = (a >> 16) & 0xff;
            shooter.pos.y += position.y;
            shooter.color = (a >> 8) & 0xff;
            shooter.spd1 = ex->r;
            shooter.spd2 = ex->s;
            shooter.start_transform = a & 0xff;
            shooter.count = ex->b;
            ex_index++;
            shooter.layers = ex[1].a;
            shooter.ang_aim = ex[1].r;
            shooter.sfx_flags = ex[1].b;
            shooter.ang_bullet_dist = ex[1].s;
            memcpy(shooter.ex, inner.ex, sizeof(inner.ex));
            g_BulletManager->shoot_bullets(&shooter);
            ex_index++;
            if ((i32)a < 0)
            {
                cancel(0, 0);
                break;
            }
            continue;
        }
        case 0x8000:
            id = ex->a;
            ex_index++;
            continue;
        case 0x10000:
            ex_index = ex->a;
            continue;
        case 0x100000:
            if (ex->a != 0)
            {
                vm_92c.flags_lo = vm_92c.flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
            }
            else
            {
                vm_92c.flags_lo &= ~ANM_VM_BLEND_MODE_MASK;
            }
            ex_index++;
            continue;
        case 0x80000000:
            ex_flags |= ex->type;
            ex_state[5].timer.set_inline(ex->a);
            break;
        }
        ex_index++;
    }
}

// Sets the laser up from its parameters: the body, origin and tip VMs, the
// delay timers, the shot sound and the start offset along the aim.
// TODO: the original realigns the frame (and esp, -8); everything else matches. So do
// the other set_vm_script callers, and set_vm_script has a padded frame there. A dead
// double in a HARNESS_CALLED AnmVm::run matches it but loses other functions; one in
// set_vm_script only makes set_vm_script realign itself (see README).
// FUNCTION: TH16 0x431b30
i32 LaserLineInf::initialize(void *params)
{
    inner = *(LaserLineInner *)params;
    bullet_type = inner.bullet_type;
    state = 2;
    kind = LASER_LINE;
    bullet_color = inner.bullet_color;
    AnmVm *vm = &vm_92c;
    vm->wipe();
    vm_92c.index_of_sprite_mapping_func = 2;
    vm_92c.associated_game_entity = this;
    g_LaserManager->bullet_anm->set_vm_script(vm, g_bullet_types[bullet_type].script);
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    AnmVmFlagsLoFields *fields = (AnmVmFlagsLoFields *)&vm_92c.flags_lo;
    fields->anchor_x = 0;
    fields->anchor_y = 2;
    fields->render_mode = 1;
    vm_92c.flags_hi = vm_92c.flags_hi & ~0x80000 | 0x40000;
    vm = &vm_f28;
    g_LaserManager->bullet_anm->copy_vm(vm, inner.bullet_color + 0x38);
    vm->unk_5b0 = NULL;
    vm->parent = NULL;
    vm->run();
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    ((AnmVmFlagsLoFields *)&vm_f28.flags_lo)->render_mode = 1;
    vm_f28.flags_hi = vm_f28.flags_hi & ~0x80000 | 0x40000;
    if (bullet_type > 0x11 && bullet_type != 0x26)
    {
        vm = &vm_1524;
        g_LaserManager->bullet_anm->copy_vm(vm, inner.bullet_color + 0x53);
        vm->unk_5b0 = NULL;
        vm->parent = NULL;
        vm->run();
    }
    else
    {
        vm = &vm_1524;
        g_LaserManager->bullet_anm->copy_vm(vm, inner.bullet_color + 0x5b);
        vm->unk_5b0 = NULL;
        vm->parent = NULL;
        vm->run();
        vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    }
    vm_1524.flags_hi = vm_1524.flags_hi & ~0x80000 | 0x40000;
    timer_5a0.set_inline(30);
    timer_5b4.set_inline(3);
    if (inner.shot_sfx >= 0)
    {
        g_SoundManager.play_sound_at_position(inner.shot_sfx, 0.0f);
    }
    timer_2c.reset();
    timer_40.reset();
    position = inner.start_pos;
    if (inner.distance != 0.0f)
    {
        Float3 offset;
        laser_sincosmul(&offset, inner.ang_aim, inner.distance);
        position.x += offset.x;
        position.y += offset.y;
    }
    width = inner.laser_new_arg_4;
    unk_70 = inner.laser_new_arg_1;
    length = inner.speed;
    angle = inner.ang_aim;
    if (inner.laser_new_arg_1 > inner.laser_new_arg_2)
    {
        unk_7c = 0.01f;
    }
    else
    {
        unk_7c = 0.0f;
    }
    laser_sincosmul(&unk_60, angle, length);
    ex_index = inner.unk_30;
    return 0;
}
