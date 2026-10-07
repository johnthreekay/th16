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
    void *fog_ptr;
    u8 unk_4[4];
    f32 fog_radius;
    f32 unk_c;
    i32 fog_color;
    f32 unk_14;
    f32 unk_18;
};

// ExpHP: zEnemyInterrupt.
struct EnemyInterrupt
{
    i32 life;
    i32 time;
    char sub_for_set_next[0x40];
    char sub_for_set_timeout[0x40];
};

// The bitfields of EnemyData::flags_low that code assigns (rather than
// sets or clears); the assignments compile to xor/and/xor.
struct EnemyFlagsLow
{
    u32 unk_0 : 2;
    // Stays alive off screen horizontally / vertically.
    u32 no_offscreen_delete_x : 1;
    u32 no_offscreen_delete_y : 1;
    u32 unk_4 : 8;
    // Set by ECL 563.
    u32 flag_1000 : 1;
    u32 unk_13 : 3;
    // Has been on screen; leaving it then deletes the enemy.
    u32 was_on_screen : 1;
    u32 unk_17 : 2;
    u32 mirrored : 1;
    // Switches anm_ids[0] between the left, right and still scripts.
    u32 directional_anm : 1;
    u32 unk_21 : 3;
    // Set while a time interrupt is running (check_time_interrupts).
    u32 flag_1000000 : 1;
    u32 unk_25 : 1;
    u32 flag_4000000 : 1;
    u32 unk_27 : 1;
    // Bomb shield up (ECL bombShield).
    u32 bombshield : 1;
    u32 unk_29 : 1;
    // Life of 1000 or more.
    u32 flag_40000000 : 1;
    u32 unk_31 : 1;
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
    D3DXVECTOR3 anm_pos_array[16];
    i32 unk_224[16];
    i32 selected_anm_index;
    i32 anm_slot_0_anm_index;
    i32 anm_slot_0_script;
    i32 anm_set_main;
    i32 unk_274;
    i32 unk_278;
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
    i32 unk_3fe0;
    i32 death_sound;
    i32 death_anm_script;
    i32 death_anm_index;
    i32 unk_3ff0;
    i32 unk_3ff4;
    i32 hit_sound;
    ZunTimer set_invuln;
    ZunTimer no_hitbox_dur;
    ZunTimer unk_4024;
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
    i32 unk_452c;

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
    i32 unk_5744;
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
