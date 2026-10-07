#pragma once

#include <string.h>

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
    // Some array of a type with a constructor lies in here: without one,
    // PlayerInner's constructor unrolls the loops over the options.
    D3DXVECTOR3 unk_4[2];
    u8 unk_1c[0x54 - 0x1c];
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
    u8 unk_d8[0xdc - 0xd8];
    // Called each frame before the option moves (with the main option of
    // the same index, even for season options).
    void(__fastcall *on_update)(PlayerOption *option);
    u8 unk_e0[0xe4 - 0xe0];
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
    // Where the bullet heads (the 0x4470f0 shot: the enemy it lined up
    // with).
    Float3 target_pos;

    struct PlayerDamageSource *damage_source();
    // 0x444e10. Fires the shooter ref names from this (free) bullet; 0 on
    // success.
    i32 create(i32 shooter_ref, i32 time, struct PlayerInner *inner);
    // 0x445e20. The default reaction to hitting an enemy: the bullet
    // stops being a damage source and plays its hit animation.
    i32 hit();
};

// Something that hurts enemies: player bullets, bombs, releases.
struct PlayerDamageSource
{
    u32 flags;
    f32 radius;
    f32 unk_8;
    // Rectangles (create_rect_damage_source): angle, then width and
    // height in unk_14 and unk_18.
    f32 unk_c;
    i32 unk_10;
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
    // Per option (8 main, then 8 season): nonzero while the option's
    // laser is out, which stops it firing more (ExpHP: index 2 is Marisa's
    // onscreen_laser_power_level).
    i32 option_lasers[0x10];
    ZunTimer iframes;
    // 0x20: damage is multiplied this frame (EnemyManager::update).
    u32 flags;
    u8 unk_16040[0x16050 - 0x16040];
    // Scaled by 1/128; aims and sizes Aya's bomb.
    f32 unk_16050;
    u8 unk_16054[0x16070 - 0x16054];
    // How far (in percent) options move toward their preferred position
    // each frame; below 30 they stay put.
    i32 percent_moved_by_options;
    // Frames since the stage ended (ExpHP: frames_after_stage_end).
    i32 unk_16074;
    // Set every frame by the autumn release.
    f32 speed_multiplier;
    u8 unk_1607c[0x16090 - 0x1607c];

    // 0x440ec0. Only the members' constructors; out of line, as the
    // original calls it for both of Player's copies and a global one.
    PlayerInner();
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
    i8 start_delay;
    i16 damage;
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
    i8 fire_rate_long;
    i8 start_delay_long;
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
    union
    {
        struct
        {
            i16 power_level_count;
            i16 max_damage_u;
        };
        // How the code reads it.
        i32 num_power_levels;
    };
    i32 power_per_level;
    i32 max_damage;
    i32 unk_2c[5];
    u8 option_pos[0x190 - 0x40];
    // Offsets from shooters until the file is loaded.
    ShtShooter *shooter_arrays[0xa];
    ShtShooter shooters[1];
};

struct Player;
extern Player *g_Player;

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
    PlayerInner snapshot_inner;
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
    i32 unk_2c790;
    u8 unk_2c794;
    u8 unk_2c795[0x2c798 - 0x2c795];
    InterpFloat player_scale_i;
    // Only used while inner.flags has 0x10.
    f32 player_scale;
    // Set every frame by the winter release.
    f32 damage_multiplier;
    i32 unk_2c7d0;
    i32 unk_2c7d4;
    i32 unk_2c7d8;
    u8 unk_2c7dc[0x2c828 - 0x2c7dc];

    Player()
    {
        memset(this, 0, sizeof(Player));
        g_Player = this;
    }
    // 0x441a50
    ~Player();
    // 0x441c60 (ExpHP: Player::operator new).
    static Player *create();
    // 0x440fb0. Loads the shot type and sets up the player; 0 on success.
    i32 initialize();
    // 0x441740 (ExpHP: Player::destroy). Puts the player back in its
    // starting state for a new stage. Works on g_Player.
    HARNESS_CALLED void reset();

    // 0x4449b0. Returns the index of the new damage source plus one.
    HARNESS_CALLED i32 create_damage_source(D3DXVECTOR3 *pos, f32 radius, f32 unk, i32 time, i32 damage);
    // 0x444b20. The same for a rectangle (ExpHP:
    // sub_444b20_prolly_creates_rectangular_damage_source).
    HARNESS_CALLED i32 create_rect_damage_source(D3DXVECTOR3 *pos, f32 width, f32 height, f32 angle, i32 unk_2, i32 damage);
    // The damage source create_damage_source returned (index plus one).
    PlayerDamageSource *get_damage_source(i32 index)
    {
        if (index == 0)
        {
            return NULL;
        }
        return &inner.damage_sources[index - 1];
    }
    void set_shoot_key_short_timer(i32 time);
    void interrupt_options();
    // 0x42ca80. Sends the options interrupt 3. Called as g_Player->.
    HARNESS_CALLED void resume_options();
    HARNESS_CALLED void set_position(f32 x, f32 y);
    // Works on g_Player (replay playback restores the position with it).
    HARNESS_CALLED void set_position_subpixel(Int2 *pos);
    // Loads a .sht file and resolves its offsets and callbacks. Does not
    // use this.
    i32 read_sht_file(ShtFile **out, const char *path);

    // The shooter a bullet's shooter_ref names.
    ShtShooter *get_shooter(i32 ref)
    {
        if (!(ref & 0xf0000))
        {
            return &sht_file->shooter_arrays[ref >> 8][(u8)ref];
        }
        return &sht_file_subseason->shooter_arrays[(u8)(ref >> 8)][(u8)ref];
    }
    // 0x443f10
    void die();
    // 0x445360. Fires one shooter if a bullet is free (and the option's
    // laser is not out); -1 if creating the bullet failed.
    i32 shoot_one_bullet(i32 shooter_ref, i32 time, PlayerInner *inner);
    // 0x445470. Fires every shooter of the current power and season level
    // whose rate matches the shot key timers.
    i32 do_shooting(i32 short_time, i32 long_time);
    // 0x4455d0. Runs the shot key timers while the player is alive.
    i32 tick_shooting_state();
    // Enters state 1 for 60 frames.
    void start_respawn();
    // 0x442380. Moves the options toward their positions around the
    // player and places their VMs.
    HARNESS_CALLED i32 update_options(PlayerOption *options, i32 count);
    // 0x442560
    i32 on_tick_body();
    static i32 __fastcall on_tick_callback(Player *player);
    static i32 __fastcall on_draw_callback(Player *player);

    // Members that reach the player through g_Player; LTCG dropped this.
    // 0x444cf0. Counts a graze at pos: effect, popup, sound and a graze
    // item flying away from the player.
    HARNESS_CALLED void do_graze(Float3 *pos);
    // Angle from pos to the player.
    HARNESS_CALLED f32 angle_to_player(Float3 *pos);
    // Whether a rectangle (pos, size) or circle hits the player: 0 no, 1
    // hit (killing the player unless invincible), 2 graze. graze_only
    // turns a hit into a graze.
    HARNESS_CALLED i32 check_hit_rect(Float3 *pos, Float3 *size, i32 graze_only);
    HARNESS_CALLED i32 check_hit_circle(Float3 *pos, f32 radius, i32 graze_only);
    // 0x443af0. The same for a rectangle reaching length from pos along
    // angle, width wide (lasers).
    HARNESS_CALLED i32 check_hit_rotated_rect(Float3 *pos, f32 angle, f32 width, f32 length, i32 graze_only);
};

// .sht files kept by ~Player when the next Player reuses them.
extern ShtFile *g_cached_sht_file;
extern ShtFile *g_cached_sht_file_subseason;

inline PlayerDamageSource *PlayerBullet::damage_source()
{
    if (damage_source_index == 0)
    {
        return NULL;
    }
    return &g_Player->inner.damage_sources[damage_source_index - 1];
}
