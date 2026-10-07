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

struct BulletOffset
{
    D3DXVECTOR2 xy;
    f32 unk_8;
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
    // Neither hurtbox nor hitbox; also hides the VMs (clear_flag_lo_2_tree).
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
    // ENEMY_FLAG_8000000.
    u32 unk_27 : 1;
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
    // 1 outside replays when Supervisor::unk_700 is set (ExpHP: DS3).
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
