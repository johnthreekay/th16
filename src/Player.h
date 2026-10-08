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

// The player: movement, options, shots and the damage they deal, getting
// hit and respawning. Layouts from ExpHP's th-re-data (zPlayer,
// zPlayerInner, zPlayerOption, zPlayerBullet, zPlayerDamageSource94,
// zShtRawFile, zShtShooter). Positions in the *_subpixel and scaled_*
// fields are in 1/128 pixels.

// PlayerInner::state.
enum PlayerState
{
    // Rising from the bottom of the screen after a death, clearing
    // bullets and lasers.
    PLAYER_STATE_RESPAWNING = 0,
    PLAYER_STATE_NORMAL = 1,
    // Lost a life: drops power, then respawns (or ends the game).
    PLAYER_STATE_DEAD = 2,
    // Nothing in TH16 enters it; on_tick_body clears lasers on its 15th
    // frame. Hits are ignored in it.
    PLAYER_STATE_3 = 3,
    // Just hit: the deathbomb window before the life is lost.
    PLAYER_STATE_HIT = 4,
};

// PlayerInner::flags.
enum PlayerFlags
{
    // Cleared by Player::reset; nothing in TH16 sets or reads it.
    PLAYER_FLAG_1 = 1 << 0,
    // The stage is over: the options fly into the player and vanish.
    PLAYER_FLAG_STAGE_ENDED = 1 << 1,
    // No shooting (during Marisa's bomb).
    PLAYER_FLAG_NO_SHOOTING = 1 << 2,
    // Getting hit plays no sound. Cleared by Player::reset; nothing in TH16
    // sets it.
    PLAYER_FLAG_SILENT_HIT = 1 << 3,
    // player_scale resizes the player and their hitbox (a DDC leftover;
    // nothing in TH16 sets it). Also stops shooting.
    PLAYER_FLAG_SCALED = 1 << 4,
    // The winter release raised the damage last frame (EnemyManager sets it
    // from Player::damage_multiplier): red afterimages.
    PLAYER_FLAG_DAMAGE_BOOSTED = 1 << 5,
};

// Player::attempted_direction (ExpHP: zPlayerDirection), an index into
// g_player_directions.
enum PlayerDirection
{
    PLAYER_DIR_NONE = 0,
    PLAYER_DIR_UP = 1,
    PLAYER_DIR_DOWN = 2,
    PLAYER_DIR_LEFT = 3,
    PLAYER_DIR_RIGHT = 4,
    PLAYER_DIR_UP_LEFT = 5,
    PLAYER_DIR_UP_RIGHT = 6,
    PLAYER_DIR_DOWN_LEFT = 7,
    PLAYER_DIR_DOWN_RIGHT = 8,
};

// Indices into PlayerInner::speeds_subpixel (and the .sht file's speeds).
enum PlayerSpeed
{
    PLAYER_SPEED_UNFOCUSED = 0,
    PLAYER_SPEED_FOCUSED = 1,
    PLAYER_SPEED_UNFOCUSED_DIAGONAL = 2,
    PLAYER_SPEED_FOCUSED_DIAGONAL = 3,
};

// Scripts of the player's own ANM file (pl0X.anm) that the player code
// starts directly.
enum PlayerAnmScript
{
    // The player sprite at rest, also restarted on getting hit.
    PLAYER_SCRIPT_IDLE = 0,
    PLAYER_SCRIPT_TURN_LEFT = 1,
    PLAYER_SCRIPT_LEFT_TO_IDLE = 2,
    PLAYER_SCRIPT_TURN_RIGHT = 3,
    PLAYER_SCRIPT_RIGHT_TO_IDLE = 4,
};

struct Int2
{
    i32 x;
    i32 y;
};

// One of the player's options: up to 4 main ones from the character's
// shot type (one per power level) and up to 8 season ones (one per season
// level). Each follows the player at an offset that depends on focus.
struct PlayerOption
{
    // Nonzero while the option is out (repopulate_options sets 2).
    i32 active;
    // Some array of a type with a constructor lies in here: without one,
    // PlayerInner's constructor unrolls the loops over the options.
    D3DXVECTOR3 unk_4[2];
    u8 unk_1c[0x54 - 0x1c];
    // Where the option heads this frame, and where it is.
    Int2 scaled_preferred_pos;
    Int2 scaled_cur_pos;
    // Its place relative to the player, unfocused ([0]) and focused ([1]),
    // from the .sht file's option positions.
    Int2 scaled_preferred_pos_rel_to_player[2];
    u8 unk_74[0x80 - 0x74];
    // Nothing in TH16 uses it (ExpHP: probably a VM id).
    i32 unk_80;
    u8 unk_84[0xa8 - 0x84];
    // The direction shooters with an angle of 995 and up fire in (nothing
    // in TH16 sets it, so 0).
    f32 angle;
    u8 unk_ac[0xb0 - 0xac];
    // The option's VM, and (main options at full power) the VM of its
    // full power look.
    AnmId anm_id;
    AnmId anm_id_full_power;
    // Nothing in TH16 uses it.
    ZunTimer timer_b8;
    u8 unk_cc[0xd0 - 0xcc];
    // Its index among the options of its kind.
    i32 index;
    // Next update moves the option straight to its preferred position.
    i32 should_instajump;
    u8 unk_d8[0xdc - 0xd8];
    // Called each frame before the option moves (with the main option of
    // the same index, even for season options). Nothing in TH16 sets it.
    void(__fastcall *on_update)(PlayerOption *option);
    u8 unk_e0[0xe4 - 0xe0];
};

// PlayerBullet::state (ExpHP: zPlayerBulletState).
enum PlayerBulletState
{
    // The slot is free.
    PLAYER_BULLET_FREE = 0,
    PLAYER_BULLET_ACTIVE = 1,
    // It hit something and plays its hit animation; it no longer hurts.
    PLAYER_BULLET_HIT = 2,
};

// PlayerBullet::flags.
enum PlayerBulletFlag
{
    // tick_bullets moves the damage source along with the bullet (Marisa's
    // laser places its own).
    PLAYER_BULLET_MOVES_DAMAGE_SOURCE = 1 << 0,
    // Fired while focused.
    PLAYER_BULLET_FOCUSED = 1 << 1,
    // A 4-bit phase used by the sideways dash shot (sht_on_tick_sideways):
    // 0 waiting for an enemy in its row, 1 lined up, 2 dashing.
    PLAYER_BULLET_PHASE_MASK = 0xf << 2,
    PLAYER_BULLET_PHASE_LINED_UP = 1 << 2,
    PLAYER_BULLET_PHASE_DASHING = 2 << 2,
};

// The bitfields of PlayerBullet::flags that code assigns.
struct PlayerBulletFlags
{
    u32 moves_damage_source : 1;
    u32 focused : 1;
    u32 unk_2 : 30;
};

// A shot fired by one shooter of a .sht file. Each owns a rectangular
// damage source that follows it.
struct PlayerBullet
{
    // PlayerBulletFlag bits.
    u32 flags;
    // Its index in PlayerInner::bullets.
    i32 index_of_self;
    AnmId anm_id;
    // Frames since it was fired.
    ZunTimer age;
    // Frames in the current phase (sht_on_tick_sideways).
    ZunTimer phase_timer;
    // Nothing in TH16 uses it.
    ZunTimer timer_34;
    PosVel pos;
    // PlayerBulletState.
    i32 state;
    // Homing shots: the id of the enemy they chase (an EnemyRef).
    i32 target_enemy_id;
    // Marisa's laser: set while it touches an enemy this frame, and set
    // while its VM plays the hitting animation (ANM interrupt 2; 3 ends it).
    i32 laser_hitting;
    i32 laser_hit_anim;
    // The damage it deals; copied to the damage source every frame.
    i32 damage;
    union
    {
        // The hitbox rectangle's size along the bullet's direction
        // (copied to the damage source's width every frame).
        f32 hitbox_width;
        // The same for Marisa's laser: its current length.
        f32 laser_length;
        // How the laser's on_init clears it.
        i32 hitbox_width_i;
    };
    // The hitbox rectangle's size across it.
    f32 hitbox_height;
    i32 unk_a8;
    // Which shooter of the .sht file fired it: index in the low byte,
    // shooter array above it, 0x10000 set for the season file.
    i32 shooter_ref;
    // Index of its damage source plus one, 0 for none.
    i32 damage_source_index;
    // Where the bullet heads (sht_on_tick_sideways: the enemy it lined up
    // with).
    Float3 target_pos;

    struct PlayerDamageSource *damage_source();
    // 0x444e10. Fires the shooter ref names from this (free) bullet; 0 on
    // success.
    i32 create(i32 shooter_ref, i32 time, struct PlayerInner *inner);
    // 0x445e20. The default reaction to hitting an enemy: the bullet
    // stops being a damage source and plays its hit animation. Returns the
    // damage.
    i32 hit();
};

// PlayerDamageSource::flags.
enum DamageSourceFlag
{
    DAMAGE_SOURCE_ACTIVE = 1 << 0,
    // A circle of radius; otherwise a rectangle (width, height, angle).
    DAMAGE_SOURCE_CIRCLE = 1 << 1,
    // Counts as bomb damage (compute_damage_to_enemy's *bomb_hit): Reimu's
    // orbs and Marisa's master spark.
    DAMAGE_SOURCE_BOMB = 1 << 2,
};

// Something that hurts enemies: player bullets, bombs, releases. A circle
// or a rotated rectangle that lives for a number of frames.
struct PlayerDamageSource
{
    // DamageSourceFlag bits.
    u32 flags;
    f32 radius;
    // Added to radius each frame.
    f32 radius_growth;
    // Rectangles: the angle of the width axis.
    f32 angle;
    union
    {
        // Added to angle each frame.
        f32 angular_speed;
        // How create_rect_damage_source clears it.
        i32 angular_speed_i;
    };
    f32 width;
    f32 height;
    PosVel pos;
    // Frames left; the source goes away at 0.
    ZunTimer lifetime;
    // Damage per hit.
    i32 damage;
    i32 total_damage_dealt;
    // The source goes away once total_damage_dealt reaches this; 9999999
    // and up means no limit.
    i32 damage_limit;
    // Hits only on frames where lifetime is a multiple of this.
    i32 hit_interval;
    // The enemy hit last (cleared every frame), so that one enemy is not
    // hit twice by the same source in a frame.
    i32 last_enemy_id;
    // The player bullet this belongs to.
    i32 bullet_index;
    // The enemy being hit while hit_func runs (nothing reads it).
    i32 hit_enemy_id;
    // Index into g_damage_source_hit_funcs, 0 for none.
    i32 hit_func;
};

// Index of the first focused shooter array of a shot type's .sht file is
// its num_power_levels + 1; season options are named by 100 + index.
#define SEASON_OPTION_INDEX_BASE 100

// PlayerBullet::shooter_ref: bit 16 picks the season .sht file (tested
// with this mask).
#define SHOOTER_REF_SEASON_MASK 0xf0000

// PlayerInner::option_lasers: the main options' slots, then the season
// options' from this index.
#define OPTION_LASER_SEASON_BASE 8

#define PLAYER_BULLET_COUNT 0x100
// damage_sources has one entry more than the code uses.
#define PLAYER_DAMAGE_SOURCE_COUNT 0x100

// The player state that LoLK kept two copies of (Player::inner and
// Player::snapshot_inner).
struct PlayerInner
{
    D3DXVECTOR3 pos;
    Int2 pos_subpixel;
    ZunTimer time_in_state;
    ZunTimer time_in_stage;
    // A second copy of time_in_stage (reset and ticked with it, ExpHP:
    // __time_in_stage__copy_3c); shooting starts once it reaches 20.
    ZunTimer shot_time_in_stage;
    PlayerOption main_options[4];
    PlayerOption subseason_options[8];
    PlayerBullet bullets[PLAYER_BULLET_COUNT];
    // Where the search for a free damage source starts.
    i32 last_created_damage_source_index;
    PlayerDamageSource damage_sources[PLAYER_DAMAGE_SOURCE_COUNT + 1];
    // PlayerState.
    i32 state;
    // The hitbox shown while focused.
    AnmId anm_id_focused_hitbox;
    // An effect (effect.anm script 0x1b) that follows the player while
    // timed_effect_timer counts down; nothing in TH16 starts the timer.
    AnmId timed_effect_anm_id;
    ZunTimer timed_effect_timer;
    i32 is_focused;
    // The shot key timers: while the key is held the short one counts
    // 0 to 14 and wraps, the long one 0 to 119; -1 when it is not held.
    ZunTimer shoot_key_short_timer;
    ZunTimer shoot_key_long_timer;
    i32 num_main_options;
    // Per option (8 main, then 8 season): the power level a laser shooter
    // fired at while its laser is out, which stops it firing more (ExpHP:
    // index 2 is Marisa's onscreen_laser_power_level).
    i32 option_lasers[0x10];
    ZunTimer iframes;
    // PlayerFlags bits.
    u32 flags;
    // The .sht file's four move speeds (PlayerSpeed) in 1/128 pixels.
    i32 speeds_subpixel[4];
    // The movement this frame in 1/128 pixels, before clamping to the
    // playfield (scaled by 1/128, it aims and sizes Aya's bomb).
    Float3 attempted_delta_pos_subpixel;
    Float3 last_nonzero_delta_pos_subpixel;
    Int2 velocity_subpixel;
    // How far (in percent) options move toward their preferred position
    // each frame; below 30 they stay put.
    i32 percent_moved_by_options;
    // Frames since PLAYER_FLAG_STAGE_ENDED was set (ExpHP:
    // frames_after_stage_end).
    i32 time_since_stage_end;
    // Set every frame by the autumn release (and Marisa's bomb).
    f32 speed_multiplier;
    // Extra movement in pixels per frame, cleared every frame (nothing in
    // TH16 sets it).
    Float3 push_velocity;
    // The power level the main options were last laid out for.
    i32 options_power_level;
    i32 num_season_options;

    // 0x440ec0. Only the members' constructors; out of line, as the
    // original calls it for both of Player's copies and a global one.
    PlayerInner();
    // 0x4440e0. Lays the options out again for the current power and
    // season levels (creating or removing them as needed), and gives the
    // main options their full power look at maximum power.
    // safebuffers: see repopulate_options (Player.cpp).
    __declspec(safebuffers) void repopulate_options();
};

struct BoundingBox3
{
    D3DXVECTOR3 min_pos;
    D3DXVECTOR3 max_pos;
};

// The shot type callbacks a .sht file refers to by index; loading the file
// replaces the indices with these pointers. The hit callbacks get the
// enemy's position and size (NULL for a circle of radius) and the
// rectangle's rotation, and return the damage dealt.
typedef i32(__fastcall *ShtBulletFunc)(PlayerBullet *bullet);
typedef i32(__fastcall *ShtHitFunc)(PlayerBullet *bullet, iptr enemy_pos, iptr enemy_size, f32 rotation,
                                    f32 radius);
typedef i32(__fastcall *DamageSourceHitFunc)(PlayerDamageSource *source, iptr enemy_pos, iptr enemy_size,
                                             f32 rotation, f32 radius);
extern ShtBulletFunc const g_sht_on_init_funcs[7];
extern ShtBulletFunc const g_sht_on_tick_funcs[8];
extern ShtHitFunc const g_sht_on_hit_funcs[8];
extern ShtBulletFunc g_sht_func_3_table[1];
extern DamageSourceHitFunc const g_damage_source_hit_funcs[3];

// ShtShooter::kind.
enum ShtShooterKind
{
    // One bullet per option that stays attached to it while the shot key
    // is held (Marisa's laser).
    SHT_SHOOTER_LASER = 2,
};

// One way of firing bullets in a .sht file (ExpHP: zShtShooter). The array
// for each power level ends with a fire_rate of -1.
struct ShtShooter
{
    // Fires when the short shot timer modulo fire_rate equals start_delay
    // (or the long timer and the *_long pair, when fire_rate_long is set).
    i8 fire_rate;
    i8 start_delay;
    i16 damage;
    Float2 offset_from_option;
    // The bullet's damage rectangle.
    Float2 hitbox;
    // From 995 up: the option's angle; from 1000 up: the option's angle
    // with a random spread and speed.
    f32 angle;
    f32 speed;
    i32 unk_1c;
    // 0: fired from the player; n: from option n - 1 (season options
    // from SEASON_OPTION_INDEX_BASE + 1).
    u8 option;
    // ShtShooterKind.
    u8 kind;
    union
    {
        struct
        {
            u8 anm;
            u8 anm_hit;
        };
        // How PlayerBullet::create reads the script number.
        i16 anm_script;
    };
    // -1 for none.
    i16 sfx_id;
    i8 fire_rate_long;
    i8 start_delay_long;
    ShtBulletFunc func_on_init;
    ShtBulletFunc func_on_tick;
    // Every .sht file leaves it 0.
    ShtBulletFunc func_3;
    ShtHitFunc func_on_hit;
    u8 unk_38[0x58 - 0x38];
};

// The number of option positions per focus state in a .sht file.
#define SHT_OPTION_POS_COUNT 21

// A shot type's .sht file (ExpHP: zShtRawFile).
struct ShtFile
{
    i16 unk_0;
    u16 shooter_array_count;
    // Player::initialize overwrites the three radii from per-character
    // tables.
    f32 hitbox_radius;
    f32 grazebox_radius;
    f32 itembox_radius;
    // Indexed by PlayerSpeed.
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
    // Player::initialize sets 100.
    i32 power_per_level;
    // compute_damage_to_enemy caps the damage of a frame at this.
    i32 max_damage;
    i32 unk_2c[5];
    // The options' places relative to the player: unfocused, then
    // focused. g_main_option_layouts and g_season_option_layouts say
    // where each level's run starts.
    Float2 option_pos[2 * SHT_OPTION_POS_COUNT];
    // The shooters of each power level (main file: unfocused, then
    // focused from num_power_levels + 1) or season level (season file).
    // Offsets from shooters until the file is loaded.
    ShtShooter *shooter_arrays[0xa];
    ShtShooter shooters[1];
};

struct Player;
extern Player *g_Player;

// The player (one, g_Player, while a stage runs). Runs as an on_tick and
// an on_draw function; inner holds the state that LoLK snapshotted.
struct Player
{
    u8 unk_0[0x4];
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    // The character's pl0X.anm and the subseason's pl0Xsub.anm.
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
    // Where the player died; while respawning, lasers are cleared in a
    // growing circle around it.
    D3DXVECTOR3 death_pos;
    // The movement this frame in 1/128 pixels (before game speed); a
    // change of sign starts the turning animations.
    Int2 attempted_velocity;
    // PlayerDirection of the arrow keys held.
    i32 attempted_direction;
    u8 unk_2c784[0x2c788 - 0x2c784];
    ShtFile *sht_file;
    ShtFile *sht_file_subseason;
    // Cleared whenever the player cannot shoot; nothing in TH16 reads
    // them.
    i32 unk_2c790;
    u8 unk_2c794;
    u8 unk_2c795[0x2c798 - 0x2c795];
    InterpFloat player_scale_i;
    // Only used while inner.flags has PLAYER_FLAG_SCALED.
    f32 player_scale;
    // Set every frame by the winter release.
    f32 damage_multiplier;
    // Cleared by reset; nothing in TH16 uses them.
    i32 unk_2c7d0;
    i32 unk_2c7d4;
    i32 unk_2c7d8;
    BoundingBox3 item_collect_box;
    BoundingBox3 item_attract_box_focused;
    BoundingBox3 item_attract_box_unfocused;
    u8 unk_2c824[0x2c828 - 0x2c824];

    Player()
    {
        memset(this, 0, sizeof(Player));
        g_Player = this;
    }
    // 0x441a50. Keeps the .sht files (and the ANM VMs) for the next Player
    // when Globals' flag says the game goes on, else unloads them.
    ~Player();
    // 0x441c60 (ExpHP: Player::operator new).
    static Player *create();
    // 0x440fb0. Loads the shot type and sets up the player; 0 on success.
    i32 initialize();
    // 0x441740 (ExpHP: Player::destroy). Puts the player back in its
    // starting state for a new stage. Works on g_Player.
    HARNESS_CALLED void reset();

    // 0x4449b0. A circle of radius growing by radius_growth per frame
    // that lives for time frames. Returns the index of the new damage
    // source plus one.
    HARNESS_CALLED i32 create_damage_source(D3DXVECTOR3 *pos, f32 radius, f32 radius_growth, i32 time, i32 damage);
    // 0x444b20. The same for a rectangle (ExpHP:
    // sub_444b20_prolly_creates_rectangular_damage_source).
    HARNESS_CALLED i32 create_rect_damage_source(D3DXVECTOR3 *pos, f32 width, f32 height, f32 angle, i32 time,
                                                 i32 damage);
    // The damage source create_damage_source returned (index plus one).
    // A ternary: the original folds the field offset of a lookup's use
    // into both arms (NULL->damage_limit becomes the constant 0x7c), which
    // the if/return form does not get (BombReimuAInf::on_tick).
    PlayerDamageSource *get_damage_source(i32 index)
    {
        return index == 0 ? NULL : &inner.damage_sources[index - 1];
    }
    // 0x440d50
    void set_shoot_key_short_timer(i32 time);
    // 0x440dc0. At the start of a stage: brings the options back
    // (clears PLAYER_FLAG_STAGE_ENDED) and sends their VMs interrupt 2.
    void start_stage_options();
    // 0x42ca80. At a stage clear: the options fly into the player and
    // vanish (PLAYER_FLAG_STAGE_ENDED), and their VMs get interrupt 3.
    // Called as g_Player->.
    HARNESS_CALLED void withdraw_options();
    // 0x440e40. Puts the player at (x, y) and the main options straight
    // into place.
    HARNESS_CALLED void set_position(f32 x, f32 y);
    // 0x4476d0. The same in 1/128 pixels. Works on g_Player (replay
    // playback restores the position with it).
    HARNESS_CALLED void set_position_subpixel(Int2 *pos);
    // 0x443790. Loads a .sht file and resolves its offsets and callbacks.
    // Does not use this.
    HARNESS_CALLED i32 read_sht_file(ShtFile **out, const char *path);

    // The option a shooter's option number (minus one) names.
    PlayerOption *get_option(i32 index)
    {
        return index >= SEASON_OPTION_INDEX_BASE ? &inner.subseason_options[index - SEASON_OPTION_INDEX_BASE]
                                                 : &inner.main_options[index];
    }

    // The shooter a bullet's shooter_ref names.
    ShtShooter *get_shooter(i32 ref)
    {
        if (!(ref & SHOOTER_REF_SEASON_MASK))
        {
            return &sht_file->shooter_arrays[ref >> 8][(u8)ref];
        }
        return &sht_file_subseason->shooter_arrays[(u8)(ref >> 8)][(u8)ref];
    }
    // 0x443f10 (ExpHP: Player::die). Getting hit: starts the deathbomb
    // window (PLAYER_STATE_HIT); lose_life follows unless the player bombs.
    void die();
    // 0x443cd0. The end of the deathbomb window: costs a life, refills the
    // bombs, drops the options and starts the death state with 180 frames
    // of invincibility.
    void lose_life();
    // 0x445360. Fires one shooter if a bullet is free (and the option's
    // laser is not out); -1 if creating the bullet failed.
    i32 shoot_one_bullet(i32 shooter_ref, i32 time, PlayerInner *inner);
    // 0x445470. Fires every shooter of the current power and season level
    // whose rate matches the shot key timers.
    i32 do_shooting(i32 short_time, i32 long_time);
    // 0x4455d0. Runs the shot key timers while the player is alive.
    i32 tick_shooting_state();
    // 0x441cf0. Reads the arrows and the focus key, moves the player and
    // the options, and keeps the hitbox and effect VMs on the player.
    i32 move();
    // 0x4456d0. Runs every live bullet: its shot type callback, movement,
    // the off-screen check and its damage source.
    i32 tick_bullets();
    // 0x444070. A deathbomb: back to the normal state, as if 60 frames
    // into it.
    void recover_from_hit();
    // 0x442380. Moves the options toward their positions around the
    // player and places their VMs.
    HARNESS_CALLED i32 update_options(PlayerOption *options, i32 count);
    // 0x442560. The player's frame: the state machine, damage sources,
    // invincibility flashing, scaling, hitboxes, shooting and bullets.
    DECOMP_NOINLINE i32 on_tick_body();
    // 0x443720, 0x443730. The update functions; on_draw draws the player
    // sprite except while dead.
    static i32 __fastcall on_tick_callback(Player *player);
    static i32 __fastcall on_draw_callback(Player *player);

    // 0x445a30. Applies the player's damage sources (and the bomb) to an
    // enemy at pos: a rectangle of size rotated by rotation, or a circle
    // of radius when size is NULL. Returns the damage (capped by the shot
    // type), sets *bomb_hit when bomb damage hit (DAMAGE_SOURCE_BOMB, or
    // BombInf::compute_damage) and *hit_pos to the last hitting
    // source. no_score skips the score and the sources' hit callbacks;
    // enemy_id stops a source hitting the same enemy twice in a frame.
    // Reaches the player through g_Player; LTCG dropped this and passes
    // rotation in xmm3.
    HARNESS_CALLED i32 compute_damage_to_enemy(Float3 *pos, Float3 *size, f32 rotation, f32 radius, i32 *bomb_hit,
                                               Float3 *hit_pos, i32 no_score, i32 enemy_id);

    // Members that reach the player through g_Player; LTCG dropped this.
    // 0x444cf0. Counts a graze at pos: effect, popup, sound and a season
    // item flying away from the player.
    HARNESS_CALLED void do_graze(Float3 *pos);
    // 0x443840. Angle from pos to the player.
    HARNESS_CALLED f32 angle_to_player(Float3 *pos);
    // 0x4438c0, 0x4439e0. Whether a rectangle (pos, size) or circle hits
    // the player: 0 no, 1 hit (calling die unless invincible), 2 graze.
    // graze_only turns a hit into a graze.
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
