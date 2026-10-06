#include <string.h>

#include "AnmManager.h"
#include "CriticalSections.h"
#include "GameErrorContext.h"
#include "GameThread.h"
#include "Globals.h"
#include "Laser.h"

// GLOBAL: TH16 0x4a6ee0
LaserManager *g_LaserManager;

// GLOBAL: TH16 0x49f2e0
BulletTypeInfo g_bullet_types[BULLET_TYPE_COUNT];

// This file's copy of ZunMath.h's sincosmul, which TH16 keeps once per
// object file. A static of its own so that it can be annotated.
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
i32 LaserDataInf::method_30(i32 a, i32 b)
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
