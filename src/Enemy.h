#pragma once

#include <d3dx9math.h>

#include "AnmVm.h"
#include "BulletManager.h"
#include "Ecl.h"
#include "Interp.h"
#include "PosVel.h"
#include "UpdateFunc.h"
#include "ZunList.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

class EnemyInf;

// ExpHP: zEnemyList.
typedef ZunList<EnemyInf> EnemyList;

// EnemyBulletShooter::aim_type (ECL etAim), thecl's names. count bullets per
// layer, layers from spd1 to spd2; AT forms add the angle to the player.
enum EtAimMode
{
    // A fan of count bullets, ang_bullet_dist apart, around ang_aim.
    ET_AIM_AT = 0,
    ET_AIM_ST = 1,
    // A ring of count bullets; each layer turns by ang_bullet_dist.
    ET_AIM_AT_RING = 2,
    ET_AIM_ST_RING = 3,
    // The ring turned by half a step, so no bullet goes at the aim.
    ET_AIM_AWAY_RING = 4,
    ET_AIM_ST_RING2 = 5,
    // Random angle within ang_bullet_dist of ang_aim.
    ET_AIM_RAND = 6,
    // A ring with random speeds between spd1 and spd1 + spd2.
    ET_AIM_RAND_SPEED_RING = 7,
    // Random angle and speed.
    ET_AIM_MEEK = 8,
    // Rings whose layers fan out alternately left and right.
    ET_AIM_AT_PYRAMID = 9,
    ET_AIM_ST_PYRAMID = 10,
    // A ring slowed by spd2 times |sin| of each bullet's angle.
    ET_AIM_PEANUT = 11,
    // The same turned by half a step.
    ET_AIM_PEANUT2 = 12,
};

// ExpHP: zEnemyBulletShooter.
struct EnemyBulletShooter
{
    i32 type;
    u32 color;
    // Where the bullets come from.
    D3DXVECTOR3 pos;
    // Aim angle and the angle between the bullets of a layer.
    f32 ang_aim;
    f32 ang_bullet_dist;
    // Speed of the first and the last layer.
    f32 spd1;
    f32 spd2;
    // Distance from pos at which bullets appear.
    f32 distance;
    // The etEx transforms the next shot gets.
    BulletEx ex[0x12];
    // laserNew's fourth argument; the first three go into pos.
    f32 laser_new_arg_4;
    u8 unk_344[0x350 - 0x344];
    // laserTiming's first four arguments.
    i32 laser_timing[4];
    // laserTiming's fifth argument; the laser instructions add a bit.
    u32 flags;
    // Bullets per layer and layers per shot.
    i16 count;
    i16 layers;
    // An EtAimMode (only the low 16 bits count).
    i32 aim_type;
    // 0x20: play shot_sfx.
    u32 sfx_flags;
    u32 shot_sfx;
    u32 shot_transform_sfx;
    u32 start_transform;
    u32 unk_37c;

    EnemyBulletShooter()
    {
        memset(this, 0, sizeof(*this));
        shot_transform_sfx = -1;
    }
};

// A shooter's offset (etOffset) or absolute origin (etOffsetAbs). For
// origins, z is 1 while the origin is set and 0 otherwise.
struct BulletOffset
{
    D3DXVECTOR2 xy;
    f32 z;
};

// EnemyLife::is_spell.
enum EnemyLifeFlags
{
    // A spell card is running: damage counts in sevenths
    // (current_scaled_by_seven).
    ENEMY_LIFE_SPELL = 1 << 0,
    // Dies at its next step_logic; nothing in TH16 sets it.
    ENEMY_LIFE_KILL = 1 << 1,
};

// ExpHP: zEnemyLife.
struct EnemyLife
{
    i32 current;
    i32 maximum;
    i32 remaining_for_cur_attack;
    // During spells, damage is counted seven times finer.
    i32 current_scaled_by_seven;
    i32 starting_value_for_next_attack;
    i32 total_damage_including_ignored;
    // EnemyLifeFlags.
    u32 is_spell;

    i32 receive_damage(i32 damage);
};

// ExpHP: zEnemyDrop.
struct EnemyDrop
{
    i32 main_type;
    i32 extra_counts[0x11];
    D3DXVECTOR2 area;

    void reset()
    {
        memset(this, 0, sizeof(*this));
        area.y = 32.0f;
        area.x = 32.0f;
        main_type = 0;
    }

    void eject_all_drops(D3DXVECTOR3 *pos);
    void eject_extra_drops(D3DXVECTOR3 *pos);
};

// ExpHP: zEnemyDropSeason.
struct EnemyDropSeason
{
    ZunTimer bonus_timer;
    i32 max_time;
    i32 min_count;
    i32 damage_per_season_drop;
    i32 damage_accounted_for_season_drops;
};

// ExpHP: zEnemyFog.
struct EnemyFog
{
    // The Fog mesh (NULL without fog).
    void *fog_ptr;
    u8 unk_4[4];
    // Target radius; cur_radius grows towards it by 2 per frame from 16.
    f32 fog_radius;
    f32 cur_radius;
    i32 fog_color;
    // Phases of the mesh's wobble, turning by pi/16 and pi/32 per frame.
    f32 wave_angle_x;
    f32 wave_angle_y;
};

// ExpHP: zEnemyInterrupt.
struct EnemyInterrupt
{
    i32 life;
    i32 time;
    char sub_for_set_next[0x40];
    char sub_for_set_timeout[0x40];
};

// EnemyData::flags_low. ECL's flagSet and flagClear take these masks; the
// numbered ones are not understood yet.
enum EnemyFlags
{
    // Takes no damage from shots and bombs.
    ENEMY_FLAG_NO_HURTBOX = 1 << 0,
    // Does not hit the player.
    ENEMY_FLAG_NO_HITBOX = 1 << 1,
    // Stays alive off screen horizontally / vertically.
    ENEMY_FLAG_OFFSCREEN_X = 1 << 2,
    ENEMY_FLAG_OFFSCREEN_Y = 1 << 3,
    // Damage is counted (total_damage_including_ignored) but not taken.
    ENEMY_FLAG_INVINCIBLE = 1 << 4,
    // Neither hurtbox nor hitbox; also hides the VMs (hide_tree).
    ENEMY_FLAG_INTANGIBLE = 1 << 5,
    // Survives reaching zero life.
    ENEMY_FLAG_NO_DEATH = 1 << 7,
    // The kill_all family kills it even with ENEMY_FLAGS_SURVIVE_KILL_ALL.
    ENEMY_FLAG_ALWAYS_KILLABLE = 1 << 8,
    // Touching it grazes (every sixth frame).
    ENEMY_FLAG_GRAZEABLE = 1 << 9,
    // One of ENEMY_FLAGS_SURVIVE_KILL_ALL; otherwise unused here.
    ENEMY_FLAG_400 = 1 << 10,
    // Any hit from the player's shots kills it at once.
    ENEMY_FLAG_DIE_ON_HIT = 1 << 11,
    // Hurtbox and hitbox are rectangles rotated by EnemyData::rotation
    // (ECL 563) instead of circles.
    ENEMY_FLAG_RECT_HITBOX = 1 << 12,
    // No damage flash or hit sound.
    ENEMY_FLAG_NO_HIT_EFFECT = 1 << 13,
    // final_pos is kept inside move_limit_center/size (moveLimit).
    ENEMY_FLAG_MOVE_LIMIT = 1 << 17,
    // EnemyData::on_tick ran this frame; EnemyManager::update clears it.
    ENEMY_FLAG_TICKED = 1 << 18,
    // Mirrored about the vertical axis: x movement and angles flip.
    ENEMY_FLAG_MIRRORED = 1 << 19,
    // anm_ids[0] switches between the still, left and right scripts.
    ENEMY_FLAG_DIRECTIONAL_ANM = 1 << 20,
    // Took damage this frame (damage flash and hit sound).
    ENEMY_FLAG_DAMAGED = 1 << 21,
    // One of ENEMY_FLAGS_SURVIVE_KILL_ALL; otherwise unused here.
    ENEMY_FLAG_400000 = 1 << 22,
    // A boss (setBoss); its id is own_boss_id.
    ENEMY_FLAG_BOSS = 1 << 23,
    // A time interrupt is running (ECL variable TIMEOUT).
    ENEMY_FLAG_TIMEOUT = 1 << 24,
    // Deleted at the next EnemyManager::update (the kill_all family).
    ENEMY_FLAG_DELETE = 1 << 25,
    // Set from EnemyCreateParams::flag_4000000 (always 0 in TH16): no
    // collision with the player, VMs drawn at final_pos without their
    // offsets, never a homing target.
    ENEMY_FLAG_4000000 = 1 << 26,
    // Set by ECL 544; never a homing target.
    ENEMY_FLAG_8000000 = 1 << 27,
    // A bomb shield (bombShield): while a bomb is active the enemy switches
    // to bombshield_on_anm_main and takes no damage.
    ENEMY_FLAG_BOMBSHIELD = 1 << 28,
    ENEMY_FLAG_BOMBSHIELD_UP = 1 << 29,
    // Created with 1000 or more life, or given its life as a boss: low-life
    // hit sound and flash like a boss.
    ENEMY_FLAG_BIG_LIFE = 1 << 30,
    // Bit 31 (ECL 549) makes the main VM flash magenta.

    // Not hurt by the player's shots (also not counted by get_enemy_count).
    ENEMY_FLAGS_UNDAMAGEABLE = ENEMY_FLAG_NO_HURTBOX | ENEMY_FLAG_INVINCIBLE | ENEMY_FLAG_INTANGIBLE,
    // Not a target of homing shots.
    ENEMY_FLAGS_UNTARGETABLE = ENEMY_FLAG_NO_HURTBOX | ENEMY_FLAG_INTANGIBLE | ENEMY_FLAG_4000000 | ENEMY_FLAG_8000000,
    // Spared by the kill_all family unless ENEMY_FLAG_ALWAYS_KILLABLE.
    ENEMY_FLAGS_SURVIVE_KILL_ALL =
        ENEMY_FLAG_INTANGIBLE | ENEMY_FLAG_NO_DEATH | ENEMY_FLAG_400 | ENEMY_FLAG_400000 | ENEMY_FLAG_BOSS,
};

// EnemyData::flags_high.
enum EnemyFlagsHigh
{
    // The VMs were given the enemy's slowdown; reset once it is over.
    ENEMY_FLAG_HIGH_VMS_SLOWED = 1 << 0,
    // Never set in TH16: not counted in enemy_count_real, and the
    // destructor leaves the VMs and the boss slot alone.
    ENEMY_FLAG_HIGH_4 = 1 << 2,
};

// The bitfields of EnemyData::flags_low that code assigns (rather than
// sets or clears); the assignments compile to xor/and/xor. See EnemyFlags.
struct EnemyFlagsLow
{
    u32 unk_0 : 2;
    u32 no_offscreen_delete_x : 1;
    u32 no_offscreen_delete_y : 1;
    u32 unk_4 : 8;
    u32 rect_hitbox : 1;
    u32 unk_13 : 3;
    // Has been on screen; leaving it then deletes the enemy.
    u32 was_on_screen : 1;
    u32 unk_17 : 2;
    u32 mirrored : 1;
    u32 directional_anm : 1;
    u32 unk_21 : 3;
    u32 timeout : 1;
    u32 unk_25 : 1;
    u32 flag_4000000 : 1;
    u32 flag_8000000 : 1;
    u32 bombshield : 1;
    u32 unk_29 : 1;
    u32 big_life : 1;
    // Makes the main VM flash magenta (ECL 549).
    u32 magenta_flash : 1;
};

// An enemy's state, embedded in EnemyInf (ExpHP: zEnemyData).
struct EnemyData
{
    PosVel prev_final_pos;
    PosVel final_pos;
    PosVel abs_pos;
    PosVel rel_pos;
    D3DXVECTOR2 hurtbox_size;
    D3DXVECTOR2 hitbox_size;
    f32 rotation;
    AnmId anm_ids[16];
    // Offset of each slot's VM from final_pos.
    D3DXVECTOR3 anm_pos_array[16];
    // Slot whose VM position each slot's VM also follows; -1 for none
    // (ECL 322).
    i32 anm_parent_slot[16];
    i32 selected_anm_index;
    i32 anm_slot_0_anm_index;
    i32 anm_slot_0_script;
    i32 anm_set_main;
    // Which way the directional main VM faces: -1 left, 0 still, 1 right.
    i32 anm_direction;
    // Group for ECL 551 (EnemyManager::kill_all_in_group), set by ECL 550.
    i32 kill_group;
    i32 anm_layers;
    D3DXVECTOR3 last_damage_pos;
    i32 ecl_int_vars[4];
    f32 ecl_float_vars[8];
    ZunTimer time_in_ecl;
    ZunTimer time_alive;
    f32 slowdown;
    EnemyList node_in_global_storage;
    InterpStrange1 abs_pos_i;
    InterpStrange1 rel_pos_i;
    InterpFloat abs_angle_i;
    InterpFloat abs_speed_i;
    InterpFloat rel_angle_i;
    InterpFloat rel_speed_i;
    InterpFloat2 abs_radial_dist_i;
    InterpFloat2 rel_radial_dist_i;
    InterpFloat2 abs_ellipse_i;
    InterpFloat2 rel_ellipse_i;
    EnemyBulletShooter bullet_props[16];
    i32 et_ex_index[16];
    BulletOffset bullet_mgr_offsets[16];
    BulletOffset bullet_mgr_origins[16];
    D3DXVECTOR2 final_sprite_size;
    D3DXVECTOR2 move_limit_center;
    D3DXVECTOR2 move_limit_size;
    i32 score_reward;
    EnemyLife life;
    EnemyDrop drops;
    // Damage added to the next frame's damage, then cleared.
    i32 pending_damage;
    i32 death_sound;
    i32 death_anm_script;
    i32 death_anm_index;
    // Frames until the next damage flash may start (4 after one).
    i32 hit_flash_timer;
    i32 unk_3ff4;
    i32 hit_sound;
    ZunTimer set_invuln;
    ZunTimer no_hitbox_dur;
    // Restarted at 30 whenever the enemy takes damage; counts down.
    ZunTimer damaged_timer;
    f32 bomb_damage_multiplier;
    EnemyDropSeason drop_season;
    u32 flags_low;
    u32 flags_high;
    i32 bombshield_on_anm_main;
    i32 bombshield_off_anm_main;
    i32 own_boss_id;
    f32 et_protect_range;
    EnemyInterrupt interrupts[8];
    EnemyInf *full;
    EnemyFog fog;
    char set_death[0x40];
    void *func_from_ecl_func_set;
    u32 is_func_set_2;
    void *func_from_ecl_flag_ext_dmg;
    void *func_from_ecl_unknown_634;
    i32 own_chapter;
    // How many enemies this one counts as in the chapter statistics
    // (setHurtbox's first call makes it 1, ECL 569 sets it); added to
    // enemies_destroyed_in_chapter once when it dies, then cleared.
    i32 chapter_count;

    EnemyData();
    // 0x41d2e0. One frame: interpolators, ECL, movement, fog and the
    // attached VMs. Nonzero once the enemy is gone.
    int on_tick();
    // 0x41bb50, 0x41c330, 0x41cbd0: on_tick's steps.
    int step_interpolators();
    int step_logic();
    void update_fog();
    // 0x41c1f0. Moves final_pos to abs_pos + rel_pos, then keeps it inside
    // the movement limit.
    void update_final_pos();
    // ECL instructions.
    // 0x423260. anmSetSprite(slot, script): replaces the VM in a slot.
    int ecl_anm_set_sprite();
    // 0x423050. The enmCreate family.
    int ecl_enm_create();
    // 0x4233a0. The anm instructions that change a VM of the enemy
    // (rotation, scale, colors, alpha, position, layer, blend mode).
    void ecl_anm_vm_instr();
    // 0x41dcb0. The enemy-specific ECL instructions (300 and up).
    int ecl_run_over_300();
    i32 get_int_arg(int index);
    i32 *get_int_arg_ptr(int index);
    f32 get_float_arg(int index);
    f32 *get_float_arg_ptr(int index);
};

// The enemy-specific ECL instructions (EnemyData::ecl_run_over_300).
// Names follow ExpHP's th-re-data labels and the usual thecl names; the
// case comments in EnemyEcl.cpp give the arguments.
enum EnemyEclOpcode
{
    ECL_OP_ENM_CREATE = 300,
    ECL_OP_ENM_CREATE_A = 301,
    ECL_OP_ANM_SELECT = 302,
    ECL_OP_ANM_SET_SPRITE = 303,
    ECL_OP_ENM_CREATE_M = 304,
    ECL_OP_ENM_CREATE_AM = 305,
    ECL_OP_ANM_SET_MAIN = 306,
    ECL_OP_ANM_PLAY = 307,
    ECL_OP_ANM_PLAY_ABS = 308,
    // The F forms do nothing while there is a boss.
    ECL_OP_ENM_CREATE_F = 309,
    ECL_OP_ENM_CREATE_AF = 310,
    ECL_OP_ENM_CREATE_MF = 311,
    ECL_OP_ENM_CREATE_AMF = 312,
    ECL_OP_ANM_SELECTED_PLAY = 313,
    ECL_OP_ANM_PLAY_HIGH = 314,
    ECL_OP_ANM_PLAY_ROTATE = 315,
    ECL_OP_ANM_316 = 316,
    ECL_OP_ANM_SWITCH = 317,
    ECL_OP_ANM_RESET = 318,
    ECL_OP_ANM_ROTATE = 319,
    ECL_OP_ANM_MOVE = 320,
    ECL_OP_ENM_CREATE_321 = 321,
    // ExpHP: enm322 (sets anm_parent_slot).
    ECL_OP_ANM_PARENT = 322,
    // ExpHP: enm323.
    ECL_OP_DEATH_ANM = 323,
    // ExpHP: enm324.
    ECL_OP_ENM_POS_OR_SELF = 324,
    ECL_OP_ANM_COLOR = 325,
    ECL_OP_ANM_COLOR_TIME = 326,
    ECL_OP_ANM_ALPHA = 327,
    ECL_OP_ANM_ALPHA_TIME = 328,
    ECL_OP_ANM_SCALE = 329,
    ECL_OP_ANM_SCALE_TIME = 330,
    ECL_OP_ANM_ALPHA2 = 331,
    ECL_OP_ANM_ALPHA2_TIME = 332,
    ECL_OP_ANM_POS_TIME = 333,
    ECL_OP_ANM_334 = 334,
    ECL_OP_ANM_SCALE2 = 335,
    ECL_OP_ANM_LAYER = 336,
    ECL_OP_ANM_BLEND_MODE = 337,
    ECL_OP_ANM_PLAY_POS = 338,
    ECL_OP_ANM_339 = 339,
    // ExpHP: anm340 (sets ENEMY_FLAG_DELETE on another enemy).
    ECL_OP_ENM_DELETE = 340,
    ECL_OP_MOVE_POS = 400,
    ECL_OP_MOVE_POS_TIME = 401,
    ECL_OP_MOVE_POS_REL = 402,
    ECL_OP_MOVE_POS_REL_TIME = 403,
    ECL_OP_MOVE_VEL = 404,
    ECL_OP_MOVE_VEL_TIME = 405,
    ECL_OP_MOVE_VEL_REL = 406,
    ECL_OP_MOVE_VEL_REL_TIME = 407,
    ECL_OP_MOVE_CIRCLE = 408,
    ECL_OP_MOVE_CIRCLE_TIME = 409,
    ECL_OP_MOVE_CIRCLE_REL = 410,
    ECL_OP_MOVE_CIRCLE_REL_TIME = 411,
    ECL_OP_MOVE_RAND = 412,
    ECL_OP_MOVE_RAND_REL = 413,
    ECL_OP_MOVE_BOSS = 414,
    ECL_OP_MOVE_BOSS_REL = 415,
    ECL_OP_MOVE_POS_3D = 416,
    ECL_OP_MOVE_POS_3D_REL = 417,
    ECL_OP_MOVE_ADD = 418,
    ECL_OP_MOVE_ADD_REL = 419,
    ECL_OP_MOVE_ELLIPSE = 420,
    ECL_OP_MOVE_ELLIPSE_TIME = 421,
    ECL_OP_MOVE_ELLIPSE_REL = 422,
    ECL_OP_MOVE_ELLIPSE_REL_TIME = 423,
    ECL_OP_MOVE_SET_MIRROR = 424,
    ECL_OP_MOVE_BEZIER = 425,
    ECL_OP_MOVE_BEZIER_REL = 426,
    ECL_OP_MOVE_RESET = 427,
    // NM: not mirrored.
    ECL_OP_MOVE_VEL_NM = 428,
    ECL_OP_MOVE_VEL_TIME_NM = 429,
    ECL_OP_MOVE_VEL_REL_NM = 430,
    ECL_OP_MOVE_VEL_REL_TIME_NM = 431,
    ECL_OP_MOVE_ENM = 432,
    ECL_OP_MOVE_ENM_REL = 433,
    ECL_OP_MOVE_CURVE = 434,
    ECL_OP_MOVE_CURVE_REL = 435,
    // Move by an offset (mirrored with the enemy).
    ECL_OP_MOVE_POS_TIME_OFFSET = 436,
    ECL_OP_MOVE_POS_REL_TIME_OFFSET = 437,
    ECL_OP_MOVE_CURVE_OFFSET = 438,
    ECL_OP_MOVE_CURVE_REL_OFFSET = 439,
    ECL_OP_MOVE_ANGLE = 440,
    ECL_OP_MOVE_ANGLE_TIME = 441,
    ECL_OP_MOVE_ANGLE_REL = 442,
    ECL_OP_MOVE_ANGLE_REL_TIME = 443,
    ECL_OP_MOVE_SPEED = 444,
    ECL_OP_MOVE_SPEED_TIME = 445,
    ECL_OP_MOVE_SPEED_REL = 446,
    ECL_OP_MOVE_SPEED_REL_TIME = 447,
    ECL_OP_SET_HURTBOX = 500,
    ECL_OP_SET_HITBOX = 501,
    ECL_OP_FLAG_SET = 502,
    ECL_OP_FLAG_CLEAR = 503,
    ECL_OP_MOVE_LIMIT = 504,
    ECL_OP_MOVE_LIMIT_RESET = 505,
    ECL_OP_DROP_CLEAR = 506,
    ECL_OP_DROP_EXTRA = 507,
    ECL_OP_DROP_AREA = 508,
    ECL_OP_DROP_ITEMS = 509,
    ECL_OP_DROP_MAIN = 510,
    ECL_OP_LIFE_SET = 511,
    ECL_OP_SET_BOSS = 512,
    ECL_OP_TIMER_RESET = 513,
    ECL_OP_SET_NEXT = 514,
    ECL_OP_SET_INVULN = 515,
    ECL_OP_PLAY_SOUND = 516,
    ECL_OP_SET_SCREEN_SHAKE = 517,
    ECL_OP_DIALOG_READ = 518,
    ECL_OP_DIALOG_WAIT = 519,
    // ExpHP: unknown520.
    ECL_OP_WAIT_BOSSES = 520,
    ECL_OP_SET_TIMEOUT = 521,
    ECL_OP_SPELL = 522,
    ECL_OP_SPELL_END = 523,
    ECL_OP_SET_CHAPTER = 524,
    ECL_OP_ENM_KILL_ALL = 525,
    ECL_OP_ET_PROTECT_RANGE = 526,
    ECL_OP_LIFE_MARKER = 527,
    ECL_OP_SPELL_528 = 528,
    ECL_OP_RANK_F3 = 529,
    ECL_OP_RANK_F5 = 530,
    ECL_OP_RANK_F2 = 531,
    ECL_OP_RANK_I3 = 532,
    ECL_OP_RANK_I5 = 533,
    ECL_OP_RANK_I2 = 534,
    ECL_OP_DIFF_I = 535,
    ECL_OP_DIFF_F = 536,
    // spell with the difficulty minus 0, 1 or 2 added to the id.
    ECL_OP_SPELL_537 = 537,
    ECL_OP_SPELL_538 = 538,
    ECL_OP_SPELL_539 = 539,
    ECL_OP_STARS = 540,
    // ExpHP: unknown541.
    ECL_OP_NO_HITBOX_TIME = 541,
    ECL_OP_SPELL_TIMEOUT = 542,
    // ExpHP: unknown543.
    ECL_OP_HIDE_BOSS_EFFECT = 543,
    // ExpHP: unknown544.
    ECL_OP_FLAG_8000000 = 544,
    ECL_OP_RESET = 545,
    ECL_OP_BOMB_SHIELD = 546,
    ECL_OP_GAME_SPEED = 547,
    ECL_OP_DIFF_WAIT = 548,
    // ExpHP: unknown549.
    ECL_OP_MAGENTA_FLASH = 549,
    // ExpHP: unknown550.
    ECL_OP_SET_KILL_GROUP = 550,
    // ExpHP: unknown551.
    ECL_OP_KILL_GROUP = 551,
    ECL_OP_Z_INDEX = 552,
    ECL_OP_HIT_SOUND = 553,
    ECL_OP_STAGE_LOGO = 554,
    ECL_OP_ENM_ALIVE = 555,
    ECL_OP_SET_DEATH = 556,
    // ExpHP: unknown557.
    ECL_OP_STAGE_FOG = 557,
    ECL_OP_FLAG_MIRROR = 558,
    // ExpHP: unknown559.
    ECL_OP_ENEMY_LIMIT = 559,
    ECL_OP_UNKNOWN_560 = 560,
    ECL_OP_DIE = 561,
    // ExpHP: unknown562.
    ECL_OP_DROP_ITEMS_ANY_MODE = 562,
    // ExpHP: unknown563.
    ECL_OP_RECT_HITBOX = 563,
    // ExpHP: unknown564.
    ECL_OP_SET_ROTATION = 564,
    ECL_OP_BOMB_INVULN = 565,
    // ExpHP: unknown566.
    ECL_OP_DIE_NOW = 566,
    // ExpHP: unknown567.
    ECL_OP_SPELL_TEXT_AT_BOTTOM = 567,
    ECL_OP_SPELL_MODE = 568,
    // ExpHP: unknown569.
    ECL_OP_CHAPTER_COUNT = 569,
    // ExpHP: unknown570.
    ECL_OP_COUNT_DESTROYED = 570,
    // ExpHP: unknown571.
    ECL_OP_KILL_ALL_NO_DEATH = 571,
    ECL_OP_LIFE_NOW = 572,
    ECL_OP_ET_NEW = 600,
    ECL_OP_ET_ON = 601,
    ECL_OP_ET_SPRITE = 602,
    ECL_OP_ET_OFFSET = 603,
    ECL_OP_ET_ANGLE = 604,
    ECL_OP_ET_SPEED = 605,
    ECL_OP_ET_COUNT = 606,
    ECL_OP_ET_AIM = 607,
    ECL_OP_ET_SOUND = 608,
    ECL_OP_ET_EX = 609,
    // etEx with c, d, m and n; the NEXT forms take the next free ex slot.
    ECL_OP_ET_EX_FULL = 610,
    ECL_OP_ET_EX_NEXT = 611,
    ECL_OP_ET_EX_FULL_NEXT = 612,
    ECL_OP_ET_CLEAR_ALL = 613,
    ECL_OP_ET_COPY = 614,
    ECL_OP_ET_CANCEL = 615,
    ECL_OP_ET_CLEAR = 616,
    ECL_OP_ET_SPEED_R3 = 617,
    ECL_OP_ET_SPEED_R5 = 618,
    ECL_OP_ET_SPEED_R2 = 619,
    ECL_OP_ET_COUNT_R3 = 620,
    ECL_OP_ET_COUNT_R5 = 621,
    ECL_OP_ET_COUNT_R2 = 622,
    ECL_OP_ANGLE_TO_PLAYER = 623,
    ECL_OP_ET_SPEED_D = 624,
    ECL_OP_ET_COUNT_D = 625,
    ECL_OP_ET_OFFSET_RAD = 626,
    ECL_OP_ET_DIST = 627,
    ECL_OP_ET_OFFSET_ABS = 628,
    ECL_OP_FOG = 629,
    ECL_OP_CALL_STD = 630,
    ECL_OP_LIFE_HIDE = 631,
    ECL_OP_FUNC_SET = 632,
    ECL_OP_FLAG_EXT_DMG = 633,
    ECL_OP_UNKNOWN_634 = 634,
    ECL_OP_ET_CANCEL2 = 635,
    ECL_OP_ET_CLEAR2 = 636,
    ECL_OP_FUNC_CALL = 637,
    ECL_OP_SCORE_ADD = 638,
    ECL_OP_FUNC_SET2 = 639,
    ECL_OP_ET_EX_SUB = 640,
    ECL_OP_ET_EX_SUBTRACT = 641,
    ECL_OP_LASER_NEW = 700,
    ECL_OP_LASER_TIMING = 701,
    ECL_OP_LASER_ON = 702,
    ECL_OP_LASER_ST_ON = 703,
    ECL_OP_LASER_OFFSET = 704,
    ECL_OP_LASER_TRAJECTORY = 705,
    ECL_OP_LASER_ST_LENGTH = 706,
    ECL_OP_LASER_ST_WIDTH = 707,
    ECL_OP_LASER_ST_ANGLE = 708,
    ECL_OP_LASER_ST_ROTATION = 709,
    ECL_OP_LASER_ST_END = 710,
    ECL_OP_LASER_CU_ON = 711,
    ECL_OP_HITBOX_RECT = 712,
    // ExpHP: unknown713.
    ECL_OP_LASER_BEAM_ON = 713,
    // ExpHP: unknown714.
    ECL_OP_LASER_714 = 714,
    ECL_OP_ENM_CALL = 800,
    ECL_OP_ENM_POS = 801,
    // ExpHP: unknown802.
    ECL_OP_BOSSES_INTERRUPT = 802,
    // Season item drops.
    ECL_OP_SPEC0 = 1000,
    ECL_OP_SPEC1 = 1001,
};

// The global variables of enemy ECL (EnemyInf::get_int_global and
// friends). Names follow ExpHP's th-re-data labels where they fit.
enum EclVar
{
    // Random nonnegative integer.
    ECL_VAR_RAND = -10000,
    // Random float in [0, 1).
    ECL_VAR_RANDF = -9999,
    // Random angle in [-pi, pi) (float only).
    ECL_VAR_RANDRAD = -9998,
    // Position the enemy is drawn at (abs_pos + rel_pos).
    ECL_VAR_FINAL_X = -9997,
    ECL_VAR_FINAL_Y = -9996,
    ECL_VAR_ABS_X = -9995,
    ECL_VAR_ABS_Y = -9994,
    ECL_VAR_REL_X = -9993,
    ECL_VAR_REL_Y = -9992,
    ECL_VAR_PLAYER_X = -9991,
    ECL_VAR_PLAYER_Y = -9990,
    // From final_pos (float only).
    ECL_VAR_ANGLE_TO_PLAYER = -9989,
    // time_in_ecl.
    ECL_VAR_TIME = -9988,
    // Random float in [-1, 1).
    ECL_VAR_RANDF2 = -9987,
    // Set while a time interrupt runs (EnemyFlagsLow::timeout).
    ECL_VAR_TIMEOUT = -9986,
    // The enemy's own variables (ecl_int_vars, ecl_float_vars).
    ECL_VAR_I0 = -9985,
    ECL_VAR_I1 = -9984,
    ECL_VAR_I2 = -9983,
    ECL_VAR_I3 = -9982,
    ECL_VAR_F0 = -9981,
    ECL_VAR_F1 = -9980,
    ECL_VAR_F2 = -9979,
    ECL_VAR_F3 = -9978,
    // Aliases of FINAL_X to REL_Y.
    ECL_VAR_FINAL_X2 = -9977,
    ECL_VAR_FINAL_Y2 = -9976,
    ECL_VAR_ABS_X2 = -9975,
    ECL_VAR_ABS_Y2 = -9974,
    ECL_VAR_REL_X2 = -9973,
    ECL_VAR_REL_Y2 = -9972,
    // Movement of abs_pos and rel_pos.
    ECL_VAR_ABS_ANGLE = -9971,
    ECL_VAR_REL_ANGLE = -9970,
    ECL_VAR_ABS_SPEED = -9969,
    ECL_VAR_REL_SPEED = -9968,
    // Radius of circular movement.
    ECL_VAR_ABS_RADIUS = -9967,
    ECL_VAR_REL_RADIUS = -9966,
    // Aliases of PLAYER_X and PLAYER_Y.
    ECL_VAR_PLAYER_X2 = -9965,
    ECL_VAR_PLAYER_Y2 = -9964,
    // Boss 0's final_pos.
    ECL_VAR_BOSS_X = -9963,
    ECL_VAR_BOSS_Y = -9962,
    // Script number of the VM in anm_ids[0] (ExpHP: ANM_ID).
    ECL_VAR_MAIN_ANM_SCRIPT = -9961,
    ECL_VAR_RANK = -9960,
    ECL_VAR_DIFF = -9959,
    // Direction of final_pos's velocity.
    ECL_VAR_FINAL_ANGLE = -9958,
    // Always 1.
    ECL_VAR_TRUE = -9957,
    // From abs_pos and rel_pos (float only).
    ECL_VAR_ABS_ANGLE_TO_PLAYER = -9956,
    ECL_VAR_REL_ANGLE_TO_PLAYER = -9955,
    ECL_VAR_LIFE = -9954,
    // 1 on that difficulty.
    ECL_VAR_EASY = -9953,
    ECL_VAR_NORMAL = -9952,
    ECL_VAR_HARD = -9951,
    ECL_VAR_LUNATIC = -9950,
    // EnemyManagerInner counters for the current spell.
    ECL_VAR_MISS_COUNT = -9949,
    ECL_VAR_BOMB_COUNT = -9948,
    ECL_VAR_CAPTURE = -9947,
    // EnemyManager::enemy_count_real.
    ECL_VAR_ENM_CNT_REAL = -9946,
    // Character plus subshot.
    ECL_VAR_SHOTTYPE = -9945,
    // Distance from final_pos to the player.
    ECL_VAR_DIST_PLAYER = -9944,
    // Boss 0's variables (the enemy's own when there is no boss).
    ECL_VAR_BOSS_I0 = -9943,
    ECL_VAR_BOSS_I1 = -9942,
    ECL_VAR_BOSS_I2 = -9941,
    ECL_VAR_BOSS_I3 = -9940,
    ECL_VAR_BOSS_F0 = -9939,
    ECL_VAR_BOSS_F1 = -9938,
    ECL_VAR_BOSS_F2 = -9937,
    ECL_VAR_BOSS_F3 = -9936,
    ECL_VAR_F4 = -9935,
    ECL_VAR_F5 = -9934,
    ECL_VAR_F6 = -9933,
    ECL_VAR_F7 = -9932,
    ECL_VAR_LAST_ENM_ID = -9931,
    ECL_VAR_POWER = -9930,
    // 1 outside replays when Supervisor::new_game_started is set (ExpHP: DS3).
    ECL_VAR_DS3 = -9927,
    // Globals shared by every enemy (EnemyManagerInner).
    ECL_VAR_GI0 = -9926,
    ECL_VAR_GI1 = -9925,
    ECL_VAR_GI2 = -9924,
    ECL_VAR_GI3 = -9923,
    ECL_VAR_GF0 = -9922,
    ECL_VAR_GF1 = -9921,
    ECL_VAR_GF2 = -9920,
    ECL_VAR_GF3 = -9919,
    ECL_VAR_GF4 = -9918,
    ECL_VAR_GF5 = -9917,
    ECL_VAR_GF6 = -9916,
    ECL_VAR_GF7 = -9915,
    // The enemy's own id.
    ECL_VAR_ID = -9914,
    // Direction of boss 0's velocity.
    ECL_VAR_BOSS_ANGLE = -9911,
    ECL_VAR_BOSS_SPEED = -9910,
    // Id of the enemy that created this one (ExpHP: UNKNOWN9).
    ECL_VAR_PARENT_ID = -9909,
    // EnemyManager::get_enemy_count.
    ECL_VAR_ENM_CNT = -9908,
    ECL_VAR_SPELL_ID = -9907,
    ECL_VAR_MIRROR = -9906,
    ECL_VAR_CHAPTER = -9905,
    // Misses in the whole game (Globals::miss_count, int only).
    ECL_VAR_GAME_MISS_COUNT = -9904,
    ECL_VAR_SUBSEASON = -9903,
};

// Damage hooks ECL can install (EnemyData::func_from_ecl_flag_ext_dmg).
typedef int(__fastcall *EnemyExtDamageFunc)(EnemyData *enemy, int damage);

// Per-frame hooks ECL can install (EnemyData::func_from_ecl_func_set); a
// nonzero result ends the enemy's tick.
typedef int(__fastcall *EnemyFuncSetFunc)(EnemyData *enemy);

// VTABLE: TH16 0x4921a8
// An enemy: an ECL VM plus its state (ExpHP: zEnemy). The name is ZUN's,
// from RTTI.
class EnemyInf : public SptInf
{
  public:
    EnemyData enemy;
    void *on_death_callback;
    i32 enemy_id;
    // Id of the enemy whose enmCreate made this one (ECL PARENT_ID).
    i32 parent_enemy_id;
    i32 unk_5748;

    EnemyInf(const char *sub_name);
    int on_tick();
    // 0x41d520. Death effects, drops and the set_death subroutine; always 1.
    int die();
    // 0x424f00 and 0x425010. The subroutine to switch to once the life or
    // time of the next interrupt is reached, NULL until then.
    const char *check_life_interrupts();
    const char *check_time_interrupts();
    void set_interrupt(int index, int time, const char *sub);
    void set_timeout(int index, const char *sub);
    virtual int run_over_300();
    virtual int get_int_global(int var);
    virtual int *get_int_global_ptr(int var);
    virtual f32 get_float_global(int var);
    virtual f32 *get_float_global_ptr(int var);
    virtual ~EnemyInf();
};
