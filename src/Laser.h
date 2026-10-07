#pragma once

#include <string.h>

#include "AnmVm.h"
#include "BulletManager.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

struct AnmLoaded;

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
    // Index into g_bullet_types, and the color within it.
    i32 bullet_type;
    i32 bullet_color;

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
    virtual i32 method_30(Float3 *pos, f32 radius);
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

// LaserDataInf's flag word (after next) with the bit curvy lasers add.
struct LaserDataFlagBits
{
    u32 ticked : 1;
    u32 pending_delete : 2;
    u32 flag_3 : 1;
    // Curvy lasers: the segments stay where they are (et_ex 0x10000000).
    u32 segments_frozen : 1;
    u32 rest : 27;
};

// Parameters of a straight laser. Layout from ExpHP (zLaserLineInner); his
// field names say which BulletManager shooter field each one comes from.
struct LaserLineInner
{
    D3DXVECTOR3 start_pos;
    f32 ang_aim;
    // ExpHP: __bmgr_00c__was_128, __bmgr_008__was_16.
    f32 laser_new_arg_2;
    f32 laser_new_arg_1;
    f32 laser_new_arg_3;
    f32 laser_new_arg_4;
    // ExpHP: spd1.
    f32 speed;
    i32 bullet_type;
    i32 bullet_color;
    f32 distance;
    i32 unk_30;
    u32 flags;
    BulletEx ex[0x12];
    i32 shot_sfx;
    i32 shot_transform_sfx;

    LaserLineInner()
    {
        memset(this, 0, sizeof(*this));
    }
};
static_assert(offsetof(LaserLineInner, ex) == 0x38, "LaserLineInner::ex");
static_assert(offsetof(LaserLineInner, shot_sfx) == 0x350, "LaserLineInner::shot_sfx");

// VTABLE: TH16 0x492424
class LaserLineInf : public LaserDataInf
{
  public:
    LaserLineInner inner;
    AnmVm vm_92c;
    AnmVm vm_f28;
    AnmVm vm_1524;

    LaserLineInf();
    // The constructor as clone has it inlined; the real one is
    // DECOMP_NOINLINE for its other callers.
    struct InlineCtor
    {
    };
    __forceinline LaserLineInf(InlineCtor)
    {
    }

    virtual void get_point(f32 distance, Float3 *out);
    DECOMP_NOINLINE virtual void run_ex();
    virtual i32 initialize(void *params);
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 on_destroy();
    virtual i32 method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f);
    virtual i32 cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e);
    virtual i32 cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d);
    virtual i32 cancel(i32 mode, i32 b);
    virtual i32 method_30(Float3 *pos, f32 radius);
    virtual i32 check_graze_or_kill(i32 a);
    virtual i32 method_3c();
    virtual i32 method_44();
    virtual i32 method_50();
    virtual LaserDataInf *clone();

    // Sprite mapping callback 2 of the line laser VMs: the sprite of the
    // laser's color (ExpHP: AnmVm::on_sprite_set__2).
    static i32 __fastcall on_sprite_set(AnmVm *vm, i32 sprite);
};

// Parameters of an infinite laser, filled in by ECL before the laser is
// created. Layout from ExpHP (zLaserInfiniteInner); his field names say
// which BulletManager shooter field each one comes from.
struct LaserInfiniteInner
{
    D3DXVECTOR3 start_pos;
    // Moves the laser's origin each frame; set by ECL laserTrajectory (z
    // always 0).
    D3DXVECTOR3 velocity;
    f32 ang_aim;
    f32 laser_st_rotation;
    f32 laser_new_arg_2;
    f32 laser_new_arg_1;
    f32 laser_new_arg_4;
    // ExpHP: spd1.
    f32 speed;
    i32 unk_30;
    i32 unk_34;
    i32 unk_38;
    i32 unk_3c;
    i32 shot_sfx;
    i32 shot_transform_sfx;
    i32 laser_st_on_arg_1;
    f32 distance;
    u8 unk_50[4];
    i32 type;
    i32 color;
    u32 flags;
    BulletEx ex[0x12];

    // Inlined into LaserInfiniteInf's constructor; the out-of-line copy is
    // at 0x411860.
    LaserInfiniteInner();
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
    virtual i32 method_30(Float3 *pos, f32 radius);
    virtual i32 check_graze_or_kill(i32 a);
};

struct LaserCurveNode
{
    LaserCurveNode *next;
    LaserCurveNode *prev;
    f32 unk_8;
    f32 unk_c;
    // 0: straight at angle/speed, 1: velocity (or turning, see 0x438370),
    // 2: speed and angle change by speed_delta/angle_delta.
    i32 mode;
    Float3 velocity;
    Float3 start_pos;
    f32 angle;
    f32 speed;
    f32 speed_delta;
    f32 angle_delta;

    LaserCurveNode()
    {
    }

    // Copies field by field (a split-off laser copies the node list); the
    // implicit copy would be one block move.
    LaserCurveNode &operator=(const LaserCurveNode &other)
    {
        next = other.next;
        prev = other.prev;
        unk_8 = other.unk_8;
        unk_c = other.unk_c;
        mode = other.mode;
        velocity = other.velocity;
        start_pos = other.start_pos;
        angle = other.angle;
        speed = other.speed;
        speed_delta = other.speed_delta;
        angle_delta = other.angle_delta;
        return *this;
    }

    // 0x438370. Steps a point of the curve back by one frame of this node's
    // motion (t is the node time, its fraction the part of the frame).
    void step_back(Float3 *out_pos, f32 *out_speed, f32 *out_angle, Float3 *pos, f32 speed, f32 angle, f32 t);
    // 0x437ee0. Where the node's motion is at the given time (unk_8 is
    // its start time), with its speed and angle there.
    void get_state(Float3 *out_pos, f32 *out_speed, f32 *out_angle, f32 time);
};

// Parameters of a curvy laser. Layout from ExpHP (zLaserCurveInner); his
// field names say which BulletManager shooter field each one comes from.
struct LaserCurveInner
{
    D3DXVECTOR3 start_pos;
    f32 ang_aim;
    f32 laser_new_arg_4;
    // ExpHP: spd1.
    f32 speed;
    i32 type;
    i32 color;
    // Number of segments (ExpHP: __bmgr_350).
    i32 segment_count;
    f32 distance;
    i32 unk_28;
    BulletEx ex[0x12];
    i32 shot_sfx;
    i32 shot_transform_sfx;
    u8 unk_34c[0x350 - 0x34c];
    // Set when a bomb splits a laser: the node list and time the new piece
    // continues from.
    LaserCurveNode *source_nodes;
    f32 source_time;

    LaserCurveInner()
    {
        memset(this, 0, sizeof(*this));
    }
};

// One point of a curvy laser's body (LaserCurveInf::unk_1524 holds
// segment_count of them).
struct LaserCurveSegment
{
    Float3 pos;
    u8 unk_c[0x18 - 0xc];
    // Direction and length of the piece to the next point.
    f32 angle;
    f32 length;
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
    DECOMP_NOINLINE virtual void run_ex();
    virtual i32 initialize(void *params);
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 on_destroy();
    virtual i32 method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f);
    virtual i32 cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e);
    virtual i32 cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d);
    virtual i32 cancel(i32 mode, i32 b);
    virtual i32 method_30(Float3 *pos, f32 radius);
    virtual i32 check_graze_or_kill(i32 a);
    virtual i32 method_3c();
    virtual i32 method_40();
    virtual i32 method_44();
    virtual i32 method_60();

    HARNESS_CALLED LaserCurveNode *append_node(f32 value);

    // Sprite mapping callback 3: one sprite per color, from 0x20c on (ExpHP:
    // AnmVm::on_sprite_set__3).
    static i32 __fastcall on_sprite_set(AnmVm *vm, i32 sprite);
};

struct LaserBeamInner
{
    D3DXVECTOR3 start_pos;
    u8 unk_c[0x18 - 0xc];
    f32 ang_aim;
    u8 unk_1c[4];
    // laser_new_arg_3.
    f32 length;
    f32 laser_new_arg_4;
    // Instruction 713's second argument.
    i32 id;
    i32 color;
    f32 distance;
    // The shooter's laser_timing[0].
    i32 timing;
    u32 flag_38 : 1;
    u32 flags_38_rest : 31;
    BulletEx ex[0x12];

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
    i32 unk_f24;
    // Filled with the length on creation.
    f32 unk_f28[0x200];
    u8 unk_1728[0x1f28 - 0x1728];

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
    virtual i32 method_30(Float3 *pos, f32 radius);
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
    // Summed by the method_1c variants: 18 to 22 for each point of a laser
    // inside their rectangle, by laser width. Nothing reads it.
    i32 unk_608;
    u8 unk_60c[4];

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
    // 0x41aa40. The laser with the given id, NULL if there is none. The
    // second argument is the same at every call site; LTCG folded it.
    HARNESS_CALLED LaserDataInf *find_by_id(i32 id, i32 unused);

    // cancel_in_rectangle as Marisa's bomb has it inlined, without the
    // count.
    __forceinline void cancel_in_rectangle_inline(Float3 *a, Float3 *b, f32 angle, i32 mode, i32 e)
    {
        LaserDataInf *laser = list_head.next;
        cancel_pos = *a;
        cancel_pos_2 = *b;
        while (laser != NULL)
        {
            LaserDataInf *next = laser->next;
            if (laser->state != 1 && laser->ticked)
            {
                laser->cancel_as_bomb_rectangle(a, b, angle, mode, e);
            }
            laser = next;
        }
    }

    // cancel_in_radius as the season releases have it inlined, without the
    // count. Our build needs the __forceinline to agree.
    __forceinline void cancel_in_radius_inline(Float3 *pos, f32 radius, i32 c, i32 d)
    {
        LaserDataInf *laser = list_head.next;
        cancel_pos = *pos;
        while (laser != NULL)
        {
            LaserDataInf *next = laser->next;
            if (laser->state != 1)
            {
                laser->cancel_as_bomb_circle(pos, radius, c, d);
            }
            laser = next;
        }
    }

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
