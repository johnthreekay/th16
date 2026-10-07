#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "UpdateFunc.h"
#include "ZunAngle.h"
#include "ZunList.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

#define BULLET_COUNT 0x7d0
#define BULLET_LAYER_COUNT 6

// The et_ex transform types (BulletEx::type). Each is one bit: a running
// transform sets its bit in Bullet::active_ex_flags until it is done, and
// the ones that run over several frames are stepped by Bullet::step_ex_NN,
// NN being the bit. Names are the usual thecl ones.
enum BulletExType
{
    // A speed boost that fades over 16 frames (step_ex_00).
    BULLET_EX_SPEEDUP = 1 << 0,
    // The spawn animation (script a + 7); only as the first transform.
    BULLET_EX_ANIM = 1 << 1,
    // Accelerate by a vector (step_ex_02).
    BULLET_EX_ACCEL = 1 << 2,
    // Turn and accelerate (step_ex_03).
    BULLET_EX_ANGLE_ACCEL = 1 << 3,
    // Stop and pick a new angle and speed, some number of times (step_ex_04).
    BULLET_EX_ANGLE = 1 << 4,
    // Bounce off the walls (step_ex_06).
    BULLET_EX_BOUNCE = 1 << 6,
    // Immune to cancels for a frames.
    BULLET_EX_INVULN = 1 << 7,
    // May leave the screen for a while (step_ex_08).
    BULLET_EX_OFFSCREEN = 1 << 8,
    // Change type and color.
    BULLET_EX_SET_SPRITE = 1 << 9,
    // Cancel the bullet (without the cancel animation if a is 1).
    BULLET_EX_DELETE = 1 << 10,
    BULLET_EX_PLAY_SOUND = 1 << 11,
    // Wrap around the playfield edges (step_ex_12).
    BULLET_EX_WRAP = 1 << 12,
    // Shoot more bullets from this one.
    BULLET_EX_SHOOT = 1 << 13,
    // Set Bullet::ex_tag (for ECL funcset 1).
    BULLET_EX_TAG = 1 << 15,
    // Jump back to transform a, b times.
    BULLET_EX_LOOP = 1 << 16,
    // Move to a position along ex_move_i (step_ex_17).
    BULLET_EX_MOVE = 1 << 17,
    // Set angle and speed at once.
    BULLET_EX_VEL = 1 << 18,
    // Move by a fixed vector for a while (step_ex_19).
    BULLET_EX_VELADD = 1 << 19,
    BULLET_EX_BLEND = 1 << 20,
    // Reach a speed and angle over time (step_ex_21).
    BULLET_EX_VELTIME = 1 << 21,
    // Interpolate the scale (scale_i).
    BULLET_EX_SIZE = 1 << 22,
    // Save position, angle and speed (ex_state[12]) for later transforms.
    BULLET_EX_SAVE = 1 << 23,
    // Spawn an enemy running the transform's string subroutine.
    BULLET_EX_ENEMY = 1 << 24,
    BULLET_EX_LAYER = 1 << 25,
    // Hidden and frozen for a frames.
    BULLET_EX_DELAY = 1 << 26,
    // Shoot a line or infinite laser.
    BULLET_EX_LASER = 1 << 27,
    // Set the hitbox size (the type's own if negative).
    BULLET_EX_HITBOX = 1 << 29,
    // Wait a frames before the next transform.
    BULLET_EX_WAIT = (i32)0x80000000,
};

// One "et_ex" transform (bullet effect) of a bullet: the arguments of ECL's
// et_ex instruction. ExpHP: zBulletEx.
struct BulletEx
{
    f32 r;
    f32 s;
    f32 m;
    f32 n;
    i32 a;
    i32 b;
    i32 c;
    i32 d;
    // A BulletExType, 0 for an empty slot.
    i32 type;
    // 0: waits until no other transform runs.
    i32 slot;
    // BULLET_EX_ENEMY's subroutine name.
    char *string;
};

// Running state of an active transform (bullets and lasers). ExpHP:
// zBulletExState.
struct BulletExState
{
    ZunTimer timer;
    f32 floats[8];
    i32 ints[5];
};

// Bullet::state.
enum BulletState
{
    BULLET_STATE_FREE = 0,
    // Flying; runs its transforms.
    BULLET_STATE_ACTIVE = 1,
    // Playing its spawn animation (BULLET_EX_ANIM); hits the player after
    // 8 frames.
    BULLET_STATE_SPAWNING = 2,
    // Hit the player: drifts at half speed while it fades.
    BULLET_STATE_HIT = 3,
    // Cancelled (Bullet::cancel): drifts while its cancel animation plays.
    BULLET_STATE_CANCELLED = 4,
    // Nothing here sets it: plays the cancel animation after 3 frames.
    BULLET_STATE_5 = 5,
    // The sentinel past the end of the bullet array.
    BULLET_STATE_SENTINEL = 6,
};

// Bullet::flags.
enum BulletFlags
{
    // Set while shot; cleared when freed.
    BULLET_FLAG_ALIVE = 1 << 0,
    // Can hit the player.
    BULLET_FLAG_HITBOX = 1 << 1,
    // Already grazed.
    BULLET_FLAG_GRAZED = 1 << 2,
    // Freed at its next tick.
    BULLET_FLAG_DELETE = 1 << 3,
    // Circular hitbox (hitbox_diameter); else a hitbox_diameter by
    // hitbox_height rectangle.
    BULLET_FLAG_ROUND_HITBOX = 1 << 4,
    // scale applies (BULLET_EX_SIZE).
    BULLET_FLAG_SCALED = 1 << 6,
    // Not ticked, only tested for grazes; nothing in TH16 sets it.
    BULLET_FLAG_FROZEN = 1 << 8,
    // Not drawn (kept out of the layer lists).
    BULLET_FLAG_NO_DRAW = 1 << 9,
};

// An enemy bullet. Layout from ExpHP (zBullet).
struct Bullet
{
    // Links free bullets; entry stays NULL.
    ZunList<Bullet> freelist_node;
    ZunList<Bullet> tick_list_node;
    u32 flags;
    i32 ex_invuln_remaining_frames;
    AnmVm vm0;
    AnmVm vm1;
    D3DXVECTOR3 pos;
    D3DXVECTOR3 velocity;
    f32 speed;
    f32 angle;
    f32 hitbox_diameter;
    // With hitbox_diameter, the size of a rectangular hitbox.
    f32 hitbox_height;
    // Position in BulletManager::bullets.
    i32 index;
    // Set by BULLET_EX_TAG. ECL funcset 1 restarts the transforms of
    // tag 1 bullets near the player at index 8 and retags them 2.
    i32 ex_tag;
    u8 unk_c50[0xc58 - 0xc50];
    // Frames the bullet may still be offscreen; 5 when shot, counts down.
    i32 offscreen_grace;
    // Script of bullet.anm played where the bullet is cancelled (none if
    // negative).
    i32 cancel_script;
    // Index of the next et_ex transform to start.
    i32 ex_index;
    // Repetitions left of the running BULLET_EX_LOOP.
    i32 ex_loop_count;
    // BulletExType bits of the running transforms.
    u32 active_ex_flags;
    // The shooter's sfx_flags; not read.
    u32 sfx_flags;
    u8 unk_c70[0xc72 - 0xc70];
    // A BulletState.
    u16 state;
    u8 unk_c74[0xc78 - 0xc74];
    // Next bullet drawn in the same layer.
    Bullet *next_in_layer;
    i32 unk_c7c;
    // Sound played when the bullet bounces off a wall (none if negative).
    i32 bounce_sound;
    i32 layer;
    BulletEx et_ex[0x12];
    BulletExState ex_state[0xe];
    u8 unk_1390[4];
    InterpFloat3 ex_move_i;
    InterpFloat scale_i;
    f32 scale;
    ZunTimer timer_1420;
    ZunTimer timer_1434;
    // Set to 60 when the bullet is shot.
    i32 unk_1448;
    // Time in the current state (reset when shot and when cancelled).
    ZunTimer state_time;
    // Ticked first thing in on_tick.
    ZunTimer time_alive;
    i16 sprite;
    i16 color;

    Bullet();
    ~Bullet();

    i32 on_tick();
    // 0x4124b0. Tests the bullet against the player (graze_only is passed
    // on): 1 if it hit (the bullet then starts its cancel animation), 2 if
    // it grazed (once per bullet).
    i32 check_player_collision(i32 graze_only);
    // 0x412670. Frees the bullet: back to the free list, off the tick list.
    void release();
    // 0x413860. Starts the et_ex transforms that are due.
    void run_ex();
    // 0x4162d0. Keeps the bullet going while it is off screen and still
    // heading back towards it, for a number of frames at most.
    i32 step_ex_08();
    // 0x414ec0. The first et_ex transform: a speed boost that fades over
    // 16 frames; 1 once it is over.
    i32 step_ex_00();
    // 0x416840. Turns the bullet into its cancel animation, dropping items
    // by mode.
    i32 cancel(i32 mode);
    // The wall bounce transform (BULLET_EX_BOUNCE) and its four walls: each
    // reflects the bullet off its wall of the bounce rectangle and returns
    // 1 if it was past it.
    i32 step_ex_06();
    i32 bounce_left();
    i32 bounce_right();
    i32 bounce_top();
    i32 bounce_bottom();
    // 0x4161f0. Moves by a fixed vector until the slot's timer reaches its
    // duration.
    i32 step_ex_19();
    // 0x4153e0. Turns and accelerates for a number of frames.
    i32 step_ex_03();
    // 0x415570. Slows to a stop over a number of frames, then picks a new
    // angle and speed, some number of times.
    i32 step_ex_04();
    // 0x414fb0 and 0x4151e0. Accelerate by a vector for a number of
    // frames, steering the angle and speed to follow the velocity.
    i32 step_ex_02();
    i32 step_ex_21();
    // 0x415d80. Wraps the bullet around to the other side once it has left
    // the playfield, some number of times.
    i32 step_ex_12();
    // 0x415f90. Moves to a position along ex_move_i, then continues at the
    // given speed.
    i32 step_ex_17();

    // ZUN's angle is a ZunAngle; some transforms call its operators.
    ZunAngle &angle_ref()
    {
        return *(ZunAngle *)&angle;
    }
};

// 0x417140. The sprite mapping callback of bullet VMs: picks the sprite for
// the bullet's type and color.
int __fastcall bullet_map_sprite(AnmVm *vm, i32 sprite);

// Owns every enemy bullet. ExpHP: zBulletManager.
struct BulletManager
{
    u8 unk_0[4];
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    Bullet *next_free;
    // Bullets to draw this frame, per layer, rebuilt every tick.
    Bullet *layer_heads[BULLET_LAYER_COUNT];
    Bullet *layer_tails[BULLET_LAYER_COUNT];
    i32 bullet_count;
    f32 et_protect_range;
    D3DXVECTOR2 ecl_unknown_560;
    u8 unk_50[8];
    i32 bullet_count_canceled_by_bombs;
    ZunList<Bullet> freelist_head;
    ZunList<Bullet> tick_list_head;
    u8 unk_7c[0x9c - 0x7c];
    // One more than BULLET_COUNT: the last one is a sentinel.
    Bullet bullets[BULLET_COUNT + 1];
    Bullet snapshot_bullets[BULLET_COUNT + 1];
    AnmId anm_ids[BULLET_COUNT + 1];
    AnmId snapshot_anm_ids[BULLET_COUNT + 1];
    i32 cancel_count;
    i32 snapshot_cancel_count;
    ZunList<Bullet> *iter_current;
    ZunList<Bullet> *iter_next;
    AnmLoaded *bullet_anm;

    BulletManager();
    ~BulletManager();

    static BulletManager *create();
    i32 initialize();
    static void destroy_all();
    // 0x416f40. Cancels every bullet without items. Works on
    // g_BulletManager; LTCG dropped this. The argument is never read
    // (callers push whatever is in ecx).
    HARNESS_CALLED void clear_all(i32 unused);
    void reset_lists();

    // Walk the tick list with iter_current/iter_next, so that the bullet
    // being ticked can unlink itself.
    Bullet *iter_first()
    {
        iter_current = tick_list_head.next;
        iter_next = iter_current != NULL ? iter_current->next : NULL;
        return iter_current != NULL ? iter_current->entry : NULL;
    }
    Bullet *iter_advance()
    {
        iter_current = iter_next;
        iter_next = iter_current != NULL ? iter_current->next : NULL;
        return iter_current != NULL ? iter_current->entry : NULL;
    }

    // Whether the counter is a multiple of n. Written as members, these
    // keep their idiv even when LTCG inlines them with a constant n.
    i32 cancel_counter_multiple_of(i32 n)
    {
        return cancel_count % n == 0;
    }
    i32 bomb_cancel_count_multiple_of(i32 n)
    {
        return bullet_count_canceled_by_bombs % n == 0;
    }

    static i32 __fastcall on_tick_callback(BulletManager *self);
    static i32 __fastcall on_draw_callback(BulletManager *self);
    i32 on_tick_body();
    i32 on_draw_body();

    // 0x416d20. Reaches the manager through its global, so LTCG drops the
    // unused this; the radius arrives in xmm2. Spares bullets that are
    // still invulnerable to cancels.
    HARNESS_CALLED i32 cancel_radius_as_bomb(D3DXVECTOR3 *pos, f32 radius, i32 mode);
    // 0x416e20. The same for a rectangle of the given size, rotated by
    // angle (xmm3).
    HARNESS_CALLED i32 cancel_rectangle_as_bomb(D3DXVECTOR3 *pos, D3DXVECTOR3 *size, f32 angle, i32 mode);
    // 0x414da0. Fires every bullet of a shot (layers x count), aimed from
    // the angle to the player. Only called through g_BulletManager.
    HARNESS_CALLED i32 shoot_bullets(struct EnemyBulletShooter *props);
    // 0x412cb0. Fires bullet i of the given layer; 1 stops the shot.
    i32 shoot_one(struct EnemyBulletShooter *props, i32 i, i32 layer, f32 angle_to_player);
    // 0x416c20. cancel_radius_as_bomb for every bullet (ECL).
    HARNESS_CALLED i32 cancel_radius(D3DXVECTOR3 *pos, f32 radius, i32 mode);
};

extern BulletManager *g_BulletManager;

// Per bullet type (lasers use it too): its ANM script and the sprite of each
// color. ExpHP has zBulletType at 0x118 bytes; the table at 0x49f2e0 has a
// stride of 0x114.
struct BulletTypeInfo
{
    i32 script;
    // Sprite remaps of bullet.anm: [color][sprite] replaces a sprite the
    // script sets ([color][0] is the color's main sprite). Types whose
    // sprites[0][0] is negative keep the script's sprites.
    i32 sprites[16][4];
    f32 hitbox_radius;
    // Draw layer of the bullet (0 to BULLET_LAYER_COUNT - 1).
    i32 layer;
    // Picks the cancel animation script: 0 by color, 1 from
    // g_bullet_cancel_scripts, 2 none, 6 the color's fourth sprite, others
    // a fixed script.
    i32 cancel_kind;
    // Script of the second VM (vm1) drawn over the bullet; 0 for none.
    i32 overlay_script;
};
static_assert(offsetof(BulletTypeInfo, hitbox_radius) == 0x104, "BulletTypeInfo layout");
static_assert(sizeof(BulletTypeInfo) == 0x114, "BulletTypeInfo layout");

#define BULLET_TYPE_COUNT 44
extern BulletTypeInfo g_bullet_types[BULLET_TYPE_COUNT];

// 0x416a00. Drops the items a cancelled bullet or laser segment leaves at
// pos, by cancel mode. LTCG passes pos in ecx and mode in edx.
HARNESS_CALLED void gen_items_from_cancel(D3DXVECTOR3 *pos, i32 mode);
