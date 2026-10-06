#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "Interp.h"
#include "PosVel.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// Layouts from ExpHP's th-re-data (zPlayer, zPlayerInner, zPlayerOption,
// zPlayerBullet, zPlayerDamageSource94). Positions in the *_subpixel fields
// are in 1/128 pixels.

struct Int2
{
    i32 x;
    i32 y;
};

// One of the player's options (main or season).
struct PlayerOption
{
    i32 active;
    u8 unk_4[0x54 - 0x4];
    Int2 scaled_preferred_pos;
    Int2 scaled_cur_pos;
    Int2 scaled_preferred_pos_rel_to_player;
    i32 unk_6c;
    i32 unk_70;
    u8 unk_74[0x80 - 0x74];
    i32 unk_80;
    u8 unk_84[0xb0 - 0x84];
    AnmId anm_id_b0;
    AnmId anm_id_b4;
    ZunTimer timer_b8;
    u8 unk_cc[0xd4 - 0xcc];
    // Next update moves the option straight to its preferred position.
    i32 should_instajump;
    u8 unk_d8[0xe4 - 0xd8];
};

struct PlayerBullet
{
    u32 flags;
    i32 index_of_self;
    AnmId anm_id;
    ZunTimer timer_c;
    ZunTimer timer_20;
    ZunTimer timer_34;
    PosVel pos;
    i32 state;
    i32 unk_90;
    i32 unk_94;
    i32 unk_98;
    i32 unk_9c;
    i32 unk_a0;
    i32 unk_a4;
    i32 unk_a8;
    // Which shooter of the .sht file fired it: index in the low byte,
    // shooter array above it, 0xf0000 set for the season file.
    i32 shooter_ref;
    // Index of its damage source plus one, 0 for none.
    i32 damage_source_index;
    u8 unk_b4[0xc0 - 0xb4];

    struct PlayerDamageSource *damage_source();
    // 0x445e20. The default reaction to hitting an enemy: the bullet
    // stops being a damage source and plays its hit animation.
    i32 hit();
};

// Something that hurts enemies: player bullets, bombs, releases.
struct PlayerDamageSource
{
    u32 flags;
    f32 radius;
    u8 unk_8[0x4];
    f32 unk_c;
    u8 unk_10[0x4];
    f32 unk_14;
    f32 unk_18;
    PosVel pos;
    ZunTimer timer_60;
    i32 damage;
    i32 total_damage_dealt;
    i32 unk_7c;
    i32 unk_80;
    i32 unk_84;
    // The player bullet this belongs to.
    i32 bullet_index;
    // Index into g_damage_source_hit_funcs.
    i32 hit_func;
    i32 unk_90;
};

struct PlayerInner
{
    D3DXVECTOR3 pos;
    Int2 pos_subpixel;
    ZunTimer time_in_state;
    ZunTimer time_in_stage;
    ZunTimer timer_3c;
    PlayerOption main_options[4];
    PlayerOption subseason_options[8];
    PlayerBullet bullets[0x100];
    i32 last_created_damage_source_index;
    PlayerDamageSource damage_sources[0x101];
    i32 state;
    AnmId anm_id_focused_hitbox;
    AnmId anm_id_15fa0;
    ZunTimer timer_15fa4;
    i32 is_focused;
    ZunTimer shoot_key_short_timer;
    ZunTimer shoot_key_long_timer;
    i32 num_main_options;
    u8 unk_15fe8[0x16028 - 0x15fe8];
    ZunTimer iframes;
    // 0x20: damage is multiplied this frame (EnemyManager::update).
    u32 flags;
    u8 unk_16040[0x16074 - 0x16040];
    i32 unk_16074;
    // Set every frame by the autumn release.
    f32 speed_multiplier;
    u8 unk_1607c[0x16090 - 0x1607c];

    // 0x4440e0
    void repopulate_options();
};

struct BoundingBox3
{
    D3DXVECTOR3 min_pos;
    D3DXVECTOR3 max_pos;
};

// The shot type callbacks a .sht file refers to by index; loading the file
// replaces the indices with these pointers.
typedef i32(__fastcall *ShtBulletFunc)(PlayerBullet *bullet);
typedef i32(__fastcall *ShtHitFunc)(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
typedef i32(__fastcall *DamageSourceHitFunc)(PlayerDamageSource *source, i32 unk, i32 enemy, f32 x, f32 y);
extern ShtBulletFunc const g_sht_on_init_funcs[7];
extern ShtBulletFunc const g_sht_on_tick_funcs[8];
extern ShtHitFunc const g_sht_on_hit_funcs[8];
extern ShtBulletFunc g_sht_func_3_table[1];
extern DamageSourceHitFunc const g_damage_source_hit_funcs[4];

// One way of firing bullets in a .sht file (ExpHP: zShtShooter). The array
// for each power level ends with a fire_rate of -1.
struct ShtShooter
{
    i8 fire_rate;
    u8 start_delay;
    u16 damage;
    Float2 offset_from_option;
    Float2 hitbox;
    f32 angle;
    f32 speed;
    i32 unk_1c;
    u8 option;
    u8 unk_21;
    u8 anm;
    u8 anm_hit;
    i16 sfx_id;
    u8 fire_rate_long;
    u8 start_delay_long;
    ShtBulletFunc func_on_init;
    ShtBulletFunc func_on_tick;
    ShtBulletFunc func_3;
    ShtHitFunc func_on_hit;
    u8 unk_38[0x58 - 0x38];
};

// A shot type's .sht file (ExpHP: zShtRawFile).
struct ShtFile
{
    i16 unk_0;
    u16 sht_off_count;
    f32 hitbox_radius;
    f32 grazebox_radius;
    f32 itembox_radius;
    f32 move_speed;
    f32 move_speed_focused;
    f32 move_speed_diagonal;
    f32 move_speed_focused_diagonal;
    i16 power_level_count;
    i16 max_damage_u;
    i32 power_per_level;
    i32 max_damage;
    i32 unk_2c[5];
    u8 option_pos[0x190 - 0x40];
    // Offsets from shooters until the file is loaded.
    ShtShooter *shooter_arrays[0xa];
    ShtShooter shooters[1];
};

struct Player
{
    u8 unk_0[0x4];
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    AnmLoaded *anm_file;
    AnmLoaded *subseason_anm_file;
    AnmVm vm;
    PlayerInner inner;
    // LoLK leftover (ExpHP: __lolk_snapshot_inner).
    u8 unk_166a0[0x2c730 - 0x166a0];
    BoundingBox3 hurtbox;
    D3DXVECTOR3 hurtbox_halfsize;
    D3DXVECTOR3 item_attract_box_unfocused_halfsize;
    D3DXVECTOR3 item_attract_box_focused_halfsize;
    D3DXVECTOR3 unk_2c76c;
    Int2 attempted_velocity;
    i32 attempted_direction;
    u8 unk_2c784[0x2c788 - 0x2c784];
    ShtFile *sht_file;
    ShtFile *sht_file_subseason;
    u8 unk_2c790[0x2c798 - 0x2c790];
    InterpFloat player_scale_i;
    // Only used while inner.flags has 0x10.
    f32 player_scale;
    // Set every frame by the winter release.
    f32 damage_multiplier;
    u8 unk_2c7d0[0x2c828 - 0x2c7d0];

    // 0x4449b0. Returns the index of the new damage source plus one.
    HARNESS_CALLED i32 create_damage_source(D3DXVECTOR3 *pos, f32 radius, f32 unk, i32 unk_2, i32 damage);
    void set_shoot_key_short_timer(i32 time);
    void interrupt_options();
    HARNESS_CALLED void set_position(f32 x, f32 y);
    // Loads a .sht file and resolves its offsets and callbacks. Does not
    // use this.
    i32 read_sht_file(ShtFile **out, const char *path);

    // The shooter a bullet's shooter_ref names.
    ShtShooter *get_shooter(i32 ref)
    {
        ShtFile *sht;
        i32 array;
        if (!(ref & 0xf0000))
        {
            sht = sht_file;
            array = ref >> 8;
        }
        else
        {
            sht = sht_file_subseason;
            array = (u8)(ref >> 8);
        }
        return &sht->shooter_arrays[array][(u8)ref];
    }
    // 0x443f10
    void die();
    // Enters state 1 for 60 frames.
    void start_respawn();
    // 0x442560
    i32 on_tick_body();
    static i32 __fastcall on_tick_callback(Player *player);
    static i32 __fastcall on_draw_callback(Player *player);

    // Members that reach the player through g_Player; LTCG dropped this.
    // Angle from pos to the player.
    HARNESS_CALLED f32 angle_to_player(Float3 *pos);
    // Whether a rectangle (pos, size) or circle hits the player: 0 no, 1
    // hit (killing the player unless invincible), 2 graze. graze_only
    // turns a hit into a graze.
    HARNESS_CALLED i32 check_hit_rect(Float3 *pos, Float3 *size, i32 graze_only);
    HARNESS_CALLED i32 check_hit_circle(Float3 *pos, f32 radius, i32 graze_only);
};

extern Player *g_Player;

inline PlayerDamageSource *PlayerBullet::damage_source()
{
    if (damage_source_index == 0)
    {
        return NULL;
    }
    return &g_Player->inner.damage_sources[damage_source_index - 1];
}
