#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "UpdateFunc.h"
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
    u8 unk_c4c[0xc68 - 0xc4c];
    u32 active_ex_flags;
    u8 unk_c6c[0xc72 - 0xc6c];
    u16 state;
    u8 unk_c74[0xc78 - 0xc74];
    // Next bullet drawn in the same layer.
    Bullet *next_in_layer;
    u8 unk_c7c[0xc84 - 0xc7c];
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
};

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
    // 0x416f40. Cancels every bullet without items. The argument is never
    // read (LTCG folded it; callers push whatever is in ecx).
    static void __stdcall clear_all(i32 unused);
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

    static i32 __fastcall on_tick_callback(BulletManager *self);
    static i32 __fastcall on_draw_callback(BulletManager *self);
    i32 on_tick_body();
    i32 on_draw_body();

    // 0x416d20. Reaches the manager through its global, so LTCG drops the
    // unused this; the radius arrives in xmm2.
    HARNESS_CALLED void cancel_radius_as_bomb(D3DXVECTOR3 *pos, f32 radius, i32 mode);
    // 0x416e20. The same for a rectangle of the given size, rotated by
    // angle (xmm3).
    HARNESS_CALLED void cancel_rectangle_as_bomb(D3DXVECTOR3 *pos, D3DXVECTOR3 *size, f32 angle, i32 mode);
};

extern BulletManager *g_BulletManager;

// Per bullet type (lasers use it too): its ANM script and the sprite of each
// color. ExpHP has zBulletType at 0x118 bytes; the table at 0x49f2e0 has a
// stride of 0x114.
struct BulletTypeInfo
{
    i32 script;
    // [color][0] is the color's sprite. Types whose sprites[0][0] is
    // negative keep the sprite their script sets.
    i32 sprites[16][4];
    f32 hitbox_radius;
    i32 unk_108;
    i32 unk_10c;
    i32 unk_110;
};

#define BULLET_TYPE_COUNT 44
extern BulletTypeInfo g_bullet_types[BULLET_TYPE_COUNT];
