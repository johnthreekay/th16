#pragma once

#include <string.h>

#include "AnmVm.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

struct AnmLoaded;

// State of one bullet/laser effect slot (ExpHP: zBulletExState).
struct BulletExState
{
    ZunTimer timer;
    f32 floats[8];
    i32 ints[5];
};

// Base of every laser kind. The name is ZUN's, from RTTI. The base methods
// are almost all empty; the slot names follow ExpHP's zVTableLaser.
// VTABLE: TH16 0x492490
class LaserDataInf
{
  public:
    LaserDataInf *prev;
    LaserDataInf *next;
    // Set once the laser has ticked.
    u32 ticked : 1;
    // Nonzero once the laser is to be removed: LaserManager counts it up
    // and deletes the laser when it reaches 2.
    u32 pending_delete : 2;
    // While set, LaserManager only runs check_graze_or_kill on the laser.
    u32 flag_3 : 1;
    u32 flags_rest : 28;
    // 1: skipped by on_draw and removed on the next tick.
    i32 state;
    i32 kind;
    ZunTimer timer;
    ZunTimer timer_2c;
    ZunTimer timer_40;
    Float3 position;
    Float3 unk_60;
    f32 angle;
    f32 unk_70;
    f32 width;
    f32 length;
    f32 unk_7c;
    i32 id;
    BulletExState ex_state[0x12];
    i32 ex_index;
    u32 ex_flags;
    i32 unk_59c;
    ZunTimer timer_5a0;
    ZunTimer timer_5b4;
    i32 countdown_5c8;
    u8 unk_5cc[0x5d4 - 0x5cc];

    LaserDataInf();

    // Point at the given distance along the laser (ExpHP: method_0).
    virtual void get_point(f32 distance, Float3 *out);
    // ExpHP: method_4.
    virtual void run_ex();
    virtual void method_8(i32 arg);
    // Sets the laser up from the parameters given to allocate_new_laser.
    virtual i32 initialize(void *params);
    // Nonzero when the laser is done.
    virtual i32 on_tick();
    virtual i32 on_draw();
    // Called right before LaserManager deletes the laser.
    virtual i32 on_destroy();
    virtual i32 method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f);
    virtual i32 cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e);
    virtual i32 cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d);
    virtual i32 cancel(i32 mode, i32 b);
    virtual i32 method_2c(i32 a, i32 b, i32 c, i32 d);
    virtual i32 method_30(i32 a, i32 b);
    virtual i32 check_graze_or_kill(i32 a);
    virtual i32 method_38();
    virtual i32 method_3c();
    virtual i32 method_40();
    virtual i32 method_44();
    virtual i32 method_48();
    virtual i32 method_4c();
    virtual i32 method_50();
    virtual i32 method_54();
    virtual i32 method_58();
    virtual i32 method_5c();
    virtual i32 method_60();
    // Only LaserLineInf implements it, returning a heap copy of itself.
    virtual LaserDataInf *clone();

    void unlink()
    {
        prev->next = next;
        if (next != NULL)
        {
            next->prev = prev;
        }
    }
};

struct LaserLineInner
{
    u8 data[0x358];

    LaserLineInner()
    {
        memset(this, 0, sizeof(*this));
    }
};

// VTABLE: TH16 0x492424
class LaserLineInf : public LaserDataInf
{
  public:
    LaserLineInner inner;
    AnmVm vm_92c;
    AnmVm vm_f28;
    AnmVm vm_1524;

    LaserLineInf();

    virtual void get_point(f32 distance, Float3 *out);
    virtual void run_ex();
    virtual i32 initialize(void *params);
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 on_destroy();
    virtual i32 method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f);
    virtual i32 cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e);
    virtual i32 cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d);
    virtual i32 cancel(i32 mode, i32 b);
    virtual i32 method_30(i32 a, i32 b);
    virtual i32 check_graze_or_kill(i32 a);
    virtual i32 method_3c();
    virtual i32 method_44();
    virtual i32 method_50();
    virtual LaserDataInf *clone();
};

struct LaserInfiniteInner
{
    u8 unk_0[0x2c];
    f32 speed;
    u8 unk_30[0x378 - 0x30];

    LaserInfiniteInner()
    {
        memset(this, 0, sizeof(*this));
        speed = 8.0f;
    }
};

// VTABLE: TH16 0x4923b8
class LaserInfiniteInf : public LaserDataInf
{
  public:
    LaserInfiniteInner inner;
    i32 unk_94c;
    AnmVm vm_950;
    AnmVm vm_f4c;

    LaserInfiniteInf();

    virtual void get_point(f32 distance, Float3 *out);
    virtual void run_ex();
    virtual i32 initialize(void *params);
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 on_destroy();
    virtual i32 method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f);
    virtual i32 cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e);
    virtual i32 cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d);
    virtual i32 cancel(i32 mode, i32 b);
    virtual i32 method_30(i32 a, i32 b);
    virtual i32 check_graze_or_kill(i32 a);
};

struct LaserCurveNode
{
    LaserCurveNode *next;
    LaserCurveNode *prev;
    f32 unk_8;
    f32 unk_c;
    u8 unk_10[0x3c - 0x10];

    LaserCurveNode()
    {
    }
};

struct LaserCurveInner
{
    u8 data[0x358];

    LaserCurveInner()
    {
        memset(this, 0, sizeof(*this));
    }
};

// VTABLE: TH16 0x4922e0
class LaserCurveInf : public LaserDataInf
{
  public:
    LaserCurveInner inner;
    AnmVm vm_92c;
    AnmVm vm_f28;
    void *unk_1524;
    void *unk_1528;
    // Head of a list of heap nodes; never a real node itself.
    LaserCurveNode nodes;

    LaserCurveInf();

    virtual void get_point(f32 distance, Float3 *out);
    virtual void run_ex();
    virtual i32 initialize(void *params);
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 on_destroy();
    virtual i32 method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f);
    virtual i32 cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e);
    virtual i32 cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d);
    virtual i32 cancel(i32 mode, i32 b);
    virtual i32 method_30(i32 a, i32 b);
    virtual i32 check_graze_or_kill(i32 a);
    virtual i32 method_3c();
    virtual i32 method_40();
    virtual i32 method_44();
    virtual i32 method_60();

    HARNESS_CALLED LaserCurveNode *append_node(f32 value);
};

struct LaserBeamInner
{
    u8 unk_0[0x38];
    u32 flag_38 : 1;
    u32 flags_38_rest : 31;
    u8 unk_3c[0x354 - 0x3c];

    LaserBeamInner()
    {
        memset(this, 0, sizeof(*this));
    }
};

// VTABLE: TH16 0x49234c
class LaserBeamInf : public LaserDataInf
{
  public:
    LaserBeamInner inner;
    AnmVm vm_928;
    u8 unk_f24[0x1f28 - 0xf24];

    LaserBeamInf();

    virtual void get_point(f32 distance, Float3 *out);
    virtual void run_ex();
    virtual void method_8(i32 arg);
    virtual i32 initialize(void *params);
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 on_destroy();
    virtual i32 method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f);
    virtual i32 cancel(i32 mode, i32 b);
    virtual i32 method_30(i32 a, i32 b);
};

// Placeholder virtual methods (not decompiled yet) live in the laser .cpp
// files rather than in src/stub/: with only the trivial LaserDataInf bodies
// visible, LTCG would speculatively devirtualize calls through the vtable.
i32 unit5_placeholder(void *object);

enum LaserKind
{
    LASER_LINE = 0,
    LASER_INFINITE = 1,
    LASER_CURVE = 2,
    LASER_BEAM = 3,
};

// Owns every live laser, kept in a list behind a dummy head.
struct LaserManager
{
    u32 unk_0;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    // Never used as a laser; memset clears even its vtable pointer.
    LaserDataInf list_head;
    LaserDataInf *list_tail;
    i32 list_length;
    // Id of the newest laser; ids start at 0x10000.
    i32 last_id;
    Float3 cancel_pos;
    Float3 cancel_pos_2;
    AnmLoaded *bullet_anm;
    u8 unk_608[8];

    LaserManager();
    ~LaserManager();
    static LaserManager *create();
    i32 initialize();
    void destroy_all();
    i32 on_tick_body();
    static i32 __fastcall on_tick_callback(LaserManager *mgr);
    static i32 __fastcall on_draw_callback(LaserManager *mgr);

    // These reach the manager through g_LaserManager; LTCG drops this.
    i32 allocate_new_laser(i32 kind, void *params);
    HARNESS_CALLED i32 cancel_in_rectangle(Float3 *a, Float3 *b, f32 angle, i32 mode, i32 e);
    i32 cancel_all();
    HARNESS_CALLED i32 cancel_in_radius(Float3 *pos, f32 radius, i32 c, i32 d);
    HARNESS_CALLED i32 clear_all(i32 mode, i32 b);

    void append(LaserDataInf *laser)
    {
        LaserDataInf *tail = list_tail;
        laser->prev = tail;
        tail->next = laser;
        list_length++;
        list_tail = laser;
    }

    void destroy(LaserDataInf *laser)
    {
        laser->on_destroy();
        list_length--;
        laser->unlink();
        if (list_tail == laser)
        {
            list_tail = laser->prev;
        }
        delete laser;
    }
};

extern LaserManager *g_LaserManager;
