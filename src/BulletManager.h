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
    i32 type;
    i32 slot;
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

enum BulletState
{
    BULLET_STATE_FREE = 0,
    BULLET_STATE_1 = 1,
    BULLET_STATE_2 = 2,
    // The sentinel past the end of the bullet array.
    BULLET_STATE_SENTINEL = 6,
};

enum BulletFlags
{
    BULLET_FLAG_SCALED = 1 << 6,
    BULLET_FLAG_100 = 1 << 8,
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
    u8 unk_c44[4];
    // Position in BulletManager::bullets.
    i32 index;
    // 1 while the bullet is active; ECL's funcset 1 cancels bullets near
    // the player by setting 2.
    i32 unk_c4c;
    u8 unk_c50[0xc5c - 0xc50];
    // Script of bullet.anm played where the bullet is cancelled (none if
    // negative).
    i32 cancel_script;
    i32 unk_c60;
    u8 unk_c64[0xc68 - 0xc64];
    u32 active_ex_flags;
    u8 unk_c6c[0xc72 - 0xc6c];
    u16 state;
    u8 unk_c74[0xc78 - 0xc74];
    // Next bullet drawn in the same layer.
    Bullet *next_in_layer;
    u8 unk_c7c[0xc80 - 0xc7c];
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
    u8 unk_1448[4];
    // Time since the bullet appeared.
    ZunTimer timer_144c;
    ZunTimer timer_1460;
    i16 sprite;
    i16 color;

    Bullet();
    ~Bullet();

    i32 on_tick();
    i32 sub_4124b0(i32 arg);
    // 0x414ec0. The first et_ex transform: a speed boost that fades over
    // 16 frames; 1 once it is over.
    i32 step_ex_00();
    // 0x416840. Turns the bullet into its cancel animation, dropping items
    // by mode.
    i32 cancel(i32 mode);
    // The wall bounce transform (et_ex type 6) and its four walls: each
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
    i32 unk_cancel_counter;
    i32 snapshot_unk_cancel_counter;
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
        return unk_cancel_counter % n == 0;
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
    i32 unk_108;
    i32 unk_10c;
    i32 unk_110;
};
static_assert(offsetof(BulletTypeInfo, hitbox_radius) == 0x104, "BulletTypeInfo layout");
static_assert(sizeof(BulletTypeInfo) == 0x114, "BulletTypeInfo layout");

#define BULLET_TYPE_COUNT 44
extern BulletTypeInfo g_bullet_types[BULLET_TYPE_COUNT];

// 0x416a00. Drops the items a cancelled bullet or laser segment leaves at
// pos, by cancel mode. LTCG passes pos in ecx and mode in edx.
HARNESS_CALLED void gen_items_from_cancel(D3DXVECTOR3 *pos, i32 mode);
