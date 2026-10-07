// The enemy-specific ECL instructions (300 and up). Instruction names are
// ExpHP's (th-re-data labels.json).
#include "Enemy.h"
#include "AnmManager.h"
#include "CriticalSections.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "Globals.h"
#include "Gui.h"
#include "PopupManager.h"
#include "SoundManager.h"
#include "BulletManager.h"
#include "Camera.h"
#include "Fog.h"
#include "GameThread.h"
#include "Laser.h"
#include "Player.h"
#include "Rng.h"
#include "ScreenEffect.h"
#include "Spellcard.h"
#include "Stage.h"
#include "Supervisor.h"
#include "ZunAngle.h"
#include "ZunAsm.h"
#include "ZunMath.h"

// create_vm, inserted at the front of the world list like create_vm_front
// (0x426160, which is a copy with pos and rotation folded). LTCG inlined
// this into the anmPlay instructions.
static __forceinline AnmId create_vm_front_at(AnmLoaded *file, i32 script, Float3 *pos, f32 rotation)
{
    ENTER_CS(CS_ANM_MANAGER);
    file->vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    file->copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    if (pos == NULL)
    {
        vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        vm->entity_pos = *pos;
    }
    vm->rotation.z = rotation;
    vm->run();
    vm->mode_of_create_child = 2;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_front(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// pi - angle, wrapped: the direction mirrored about the vertical axis, for
// mirrored enemies (ENEMY_FLAG_MIRRORED).
static inline f32 mirror_angle(f32 angle)
{
    return normalize_angle(ZUN_PI / 2 - normalize_angle(angle - ZUN_PI / 2));
}

// The argument for the current difficulty, out of four starting at first.
#define ECL_DIFF_ARG(first)                                                                                        \
    (g_Globals.difficulty == DIFFICULTY_EASY     ? (first)                                                         \
     : g_Globals.difficulty == DIFFICULTY_NORMAL ? (first) + 1                                                     \
     : g_Globals.difficulty == DIFFICULTY_HARD   ? (first) + 2                                                     \
                                                 : (first) + 3)

// The bitfields of EnemyLife::is_spell that ECL assigns.
struct EnemyLifeSpellBits
{
    u32 active : 1;
    u32 unk_1 : 31;
};

// The bitfields of Spellcard::flags that ECL assigns (xor/and/xor).
struct SpellcardFlagBits
{
    u32 unk_0 : 8;
    u32 text_at_bottom : 1;
    u32 unk_9 : 23;
};

extern EnemyFuncSetFunc const g_ecl_func_sets[3];
int __fastcall ecl_ext_damage_stored(EnemyData *enemy, int damage);
int __fastcall ecl_ext_damage_anm_hurtbox(EnemyData *enemy, int damage);

// The damage hooks ECL's flagExtDmg installs.
// GLOBAL: TH16 0x490eb4
extern EnemyExtDamageFunc const g_ecl_ext_damage_funcs[3] = {NULL, ecl_ext_damage_stored, ecl_ext_damage_anm_hurtbox};

// The hooks ECL 634 installs; only entry 0 (NULL).
// GLOBAL: TH16 0x4a6dc4
void *g_ecl_unknown_634_funcs[1];

// This file's copy of sincosmul (ZunAsm.h; TH16 keeps one per object
// file).
// FUNCTION: TH16 0x426260
static void __fastcall ecl_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    ZUN_ASM_SINCOSMUL(dst, angle, radius);
}

// Where shooter i fires from: its absolute origin plus the offset when the
// origin is set (third component above 0.9), else the enemy's position plus
// the offset. Written straight into the destination: a returned vector
// would be a stack temporary, which makes LTCG align the frame.
static __forceinline void get_shot_origin(EnemyData *enemy, i32 i, D3DXVECTOR3 *out)
{
    if (enemy->bullet_mgr_origins[i].z > 0.9f)
    {
        *out = D3DXVECTOR3(enemy->bullet_mgr_origins[i].xy.x + enemy->bullet_mgr_offsets[i].xy.x,
                           enemy->bullet_mgr_origins[i].xy.y + enemy->bullet_mgr_offsets[i].xy.y, 0.0f);
    }
    else
    {
        *out = D3DXVECTOR3(enemy->bullet_mgr_offsets[i].xy.x + enemy->final_pos.pos.x,
                           enemy->bullet_mgr_offsets[i].xy.y + enemy->final_pos.pos.y,
                           enemy->bullet_mgr_offsets[i].z + enemy->final_pos.pos.z);
    }
}

// setNext: the life at which interrupt index fires and, unless the life is
// negative, its time and subroutine.
static inline void set_next(EnemyInf *enemy, int index, int life, int time, const char *sub)
{
    enemy->enemy.interrupts[index].life = life;
    if (life >= 0)
    {
        enemy->set_interrupt(index, time, sub);
    }
}

// The fog instruction's `new Fog(0, 0x11, 0)`, kept out of line for now.
// The original constructs the fog in ecl_run_over_300 itself, which gives
// that function an EH frame; in our partial program that frame makes LTCG
// stop inlining the UCRT math into zun_fabsf, zun_cosf, zun_sinf,
// zun_floorf and shoot_bullets (they lose their matches). That happens to
// everything called, directly or not, from a function with an EH frame
// and no frame realignment: zun_fabsf and shoot_bullets are called from
// here, the other three through PosVel::step. Once more of the program
// exists, try moving it back.
static DECOMP_NOINLINE Fog *new_enemy_fog()
{
    return new Fog(0, 0x11, 0);
}

// The laser instructions and angleToPlayer, kept out of line for now
// because of new_enemy_fog. Without its EH frame, LTCG realigns the frame
// of ecl_run_over_300 (and esp, -8) once enough of it wants 8-byte
// alignment: these four parameter blocks together with the two zun_atan2f
// calls tip it. A realigned caller makes LTCG lay out Spellcard::end and
// BulletManager::cancel_rectangle_as_bomb for the stack alignment it then
// knows (an 8-byte frame, no shrink-wrapping), and both lose their matches.
// Move these back together with the fog.

// laserOn(et): a line laser from the shooter's settings.
static DECOMP_NOINLINE void ecl_laser_on(EnemyData *enemy)
{
    LaserLineInner params;
    i32 idx = enemy->get_int_arg(0);
    memcpy(params.ex, enemy->bullet_props[idx].ex, sizeof(params.ex));
    get_shot_origin(enemy, idx, &params.start_pos);
    params.bullet_type = enemy->bullet_props[idx].type;
    params.bullet_color = enemy->bullet_props[idx].color;
    params.ang_aim = normalize_angle(enemy->bullet_props[idx].ang_aim);
    params.flags = enemy->bullet_props[idx].flags | 1;
    params.speed = enemy->bullet_props[idx].spd1;
    params.laser_new_arg_1 = enemy->bullet_props[idx].pos.x;
    params.laser_new_arg_2 = enemy->bullet_props[idx].pos.y;
    params.shot_sfx = enemy->bullet_props[idx].shot_sfx;
    params.laser_new_arg_3 = enemy->bullet_props[idx].pos.z;
    params.shot_transform_sfx = enemy->bullet_props[idx].shot_transform_sfx;
    params.laser_new_arg_4 = enemy->bullet_props[idx].laser_new_arg_4;
    params.distance = enemy->bullet_props[idx].distance;
    g_LaserManager->allocate_new_laser(LASER_LINE, &params);
}

// laserStOn(et, a): an infinite laser.
static DECOMP_NOINLINE void ecl_laser_st_on(EnemyData *enemy)
{
    LaserInfiniteInner params;
    i32 idx = enemy->get_int_arg(0);
    memcpy(params.ex, enemy->bullet_props[idx].ex, sizeof(params.ex));
    get_shot_origin(enemy, idx, &params.start_pos);
    params.type = enemy->bullet_props[idx].type;
    params.color = enemy->bullet_props[idx].color;
    params.ang_aim = normalize_angle(enemy->bullet_props[idx].ang_aim);
    params.start_time = enemy->bullet_props[idx].laser_timing[0];
    params.expand_time = enemy->bullet_props[idx].laser_timing[1];
    params.speed = enemy->bullet_props[idx].spd1;
    params.duration = enemy->bullet_props[idx].laser_timing[2];
    params.laser_new_arg_1 = enemy->bullet_props[idx].pos.x;
    params.shrink_time = enemy->bullet_props[idx].laser_timing[3];
    params.flags = enemy->bullet_props[idx].flags | 2;
    params.laser_new_arg_2 = enemy->bullet_props[idx].pos.y;
    params.laser_new_arg_4 = enemy->bullet_props[idx].laser_new_arg_4;
    params.shot_sfx = enemy->bullet_props[idx].shot_sfx;
    params.distance = enemy->bullet_props[idx].distance;
    params.shot_transform_sfx = enemy->bullet_props[idx].shot_transform_sfx;
    params.laser_st_on_arg_1 = enemy->get_int_arg(1);
    g_LaserManager->allocate_new_laser(LASER_INFINITE, &params);
}

// A beam laser (ExpHP: unknown713).
static DECOMP_NOINLINE void ecl_laser_beam_on(EnemyData *enemy)
{
    LaserBeamInner params;
    i32 idx = enemy->get_int_arg(0);
    memcpy(params.ex, enemy->bullet_props[idx].ex, sizeof(params.ex));
    get_shot_origin(enemy, idx, &params.start_pos);
    params.color = enemy->bullet_props[idx].color;
    params.ang_aim = normalize_angle(enemy->bullet_props[idx].ang_aim);
    params.timing = enemy->bullet_props[idx].laser_timing[0];
    params.laser_new_arg_4 = enemy->bullet_props[idx].laser_new_arg_4;
    params.length = enemy->bullet_props[idx].pos.z;
    params.distance = enemy->bullet_props[idx].distance;
    params.id = enemy->get_int_arg(1);
    g_LaserManager->allocate_new_laser(LASER_BEAM, &params);
}

// laserCuOn(et): a curvy laser.
static DECOMP_NOINLINE void ecl_laser_cu_on(EnemyData *enemy)
{
    LaserCurveInner params;
    i32 idx = enemy->get_int_arg(0);
    memcpy(params.ex, enemy->bullet_props[idx].ex, sizeof(params.ex));
    get_shot_origin(enemy, idx, &params.start_pos);
    params.type = enemy->bullet_props[idx].type;
    params.color = enemy->bullet_props[idx].color;
    params.ang_aim = normalize_angle(enemy->bullet_props[idx].ang_aim);
    params.segment_count = enemy->bullet_props[idx].laser_timing[0];
    params.unk_28 |= 1;
    params.shot_sfx = enemy->bullet_props[idx].shot_sfx;
    params.speed = enemy->bullet_props[idx].spd1;
    params.shot_transform_sfx = enemy->bullet_props[idx].shot_transform_sfx;
    params.laser_new_arg_4 = enemy->bullet_props[idx].laser_new_arg_4;
    params.distance = enemy->bullet_props[idx].distance;
    g_LaserManager->allocate_new_laser(LASER_CURVE, &params);
}

// angleToPlayer(var, x, y)
static DECOMP_NOINLINE void ecl_angle_to_player(EnemyData *enemy)
{
    *enemy->get_float_arg_ptr(0) =
        zun_atan2f(g_Player->inner.pos.y - enemy->get_float_arg(2), g_Player->inner.pos.x - enemy->get_float_arg(1));
}

// TODO: the original has an EH frame (from the fog instruction's new Fog)
// and the laser instructions and angleToPlayer inline. Ours has no EH frame
// and those out of line (see new_enemy_fog and ecl_laser_on); with all of
// them back the function reaches about 64% in quickdiff. The early
// argument getters are spelled out because LTCG inlined only those (its
// inline budget ran out in moveVelTime), plus the three in the spell case.
// FUNCTION: TH16 0x41dcb0
int EnemyData::ecl_run_over_300()
{
    EclRawInstr *instr = full->context.current_context->current_instr();
    switch ((i16)instr->opcode)
    {
    // enmCreateF and its variants: only without a boss.
    case ECL_OP_ENM_CREATE_F:
    case ECL_OP_ENM_CREATE_AF:
    case ECL_OP_ENM_CREATE_MF:
    case ECL_OP_ENM_CREATE_AMF:
        if (g_EnemyManager->get_boss(0) != NULL)
        {
            break;
        }
    // enmCreate, enmCreateA, enmCreateM, enmCreateAM, enmCreate321.
    case ECL_OP_ENM_CREATE:
    case ECL_OP_ENM_CREATE_A:
    case ECL_OP_ENM_CREATE_M:
    case ECL_OP_ENM_CREATE_AM:
    case ECL_OP_ENM_CREATE_321:
        ecl_enm_create();
        break;
    // anmSelect(file)
    case ECL_OP_ANM_SELECT:
        selected_anm_index = full->context.current_context->get_int_arg(0);
        break;
    // moveSetMirror(mirrored)
    case ECL_OP_MOVE_SET_MIRROR:
        ((EnemyFlagsLow *)&flags_low)->mirrored = full->context.current_context->get_int_arg(0);
        break;
    // lifeHide(hide)
    case ECL_OP_LIFE_HIDE:
        g_EnemyManager->inner.life_bar_hidden = full->context.current_context->get_int_arg(0);
        break;
    // zIndex(layer)
    case ECL_OP_Z_INDEX:
        anm_layers = full->context.current_context->get_int_arg(0);
        break;
    // hitSound(sound)
    case ECL_OP_HIT_SOUND:
        hit_sound = full->context.current_context->get_int_arg(0);
        break;
    // scoreAdd(points)
    case ECL_OP_SCORE_ADD:
    {
        i32 points = full->context.current_context->get_int_arg(0);
        g_Globals.add_to_score(points);
        g_PopupManager->generate_small_score_popup(&final_pos.pos, points, -1);
        break;
    }
    // anmSetSprite(slot, script)
    case ECL_OP_ANM_SET_SPRITE:
        ecl_anm_set_sprite();
        break;
    // anmMove(slot, x, y): the offset of a slot's VM.
    case ECL_OP_ANM_MOVE:
    {
        i32 slot = full->context.current_context->get_int_arg(0);
        anm_pos_array[slot].x = full->context.current_context->get_float_arg(1);
        anm_pos_array[slot].y = full->context.current_context->get_float_arg(2);
        anm_pos_array[slot].z = 0.0f;
        break;
    }
    // The instructions that change one of the enemy's VMs.
    case ECL_OP_ANM_ROTATE:
    case ECL_OP_ANM_COLOR:
    case ECL_OP_ANM_COLOR_TIME:
    case ECL_OP_ANM_ALPHA:
    case ECL_OP_ANM_ALPHA_TIME:
    case ECL_OP_ANM_SCALE:
    case ECL_OP_ANM_SCALE_TIME:
    case ECL_OP_ANM_ALPHA2:
    case ECL_OP_ANM_ALPHA2_TIME:
    case ECL_OP_ANM_POS_TIME:
    case ECL_OP_ANM_SCALE2:
    case ECL_OP_ANM_LAYER:
    case ECL_OP_ANM_BLEND_MODE:
        ecl_anm_vm_instr();
        break;
    // unknown566: dies now.
    case ECL_OP_DIE_NOW:
        if (full->die())
        {
            return 1;
        }
        break;
    // enm323(file, script): the death animation.
    case ECL_OP_DEATH_ANM:
        death_anm_index = full->context.current_context->get_int_arg(0);
        death_anm_script = full->context.current_context->get_int_arg(1);
        break;
    // die: the death sound and animation without dying.
    case ECL_OP_DIE:
        if (death_sound >= 0)
        {
            g_SoundManager.play_sound_at_position(death_sound, final_pos.pos.x);
        }
        if (death_anm_script >= 0)
        {
            f32 angle = -ZUN_PI / 2;
            Float3 *pos = &final_pos.pos;
            if (!(0.04f > (last_damage_pos.x - pos->x) * (last_damage_pos.x - pos->x) +
                              (last_damage_pos.y - pos->y) * (last_damage_pos.y - pos->y)))
            {
                angle = zun_atan2f(pos->y - last_damage_pos.y, pos->x - last_damage_pos.x);
            }
            g_EnemyManager->anim_statement_anms[death_anm_index]->create_vm(death_anm_script, pos, angle, 3, 0);
        }
        break;
    // enm322(index, value)
    case ECL_OP_ANM_PARENT:
        anm_parent_slot[full->context.current_context->get_int_arg(0)] = full->context.current_context->get_int_arg(1);
        break;
    // stageLogo
    case ECL_OP_STAGE_LOGO:
        show_stage_logo();
        break;
    // anmPlayPos(file, script, x, y, rotation)
    case ECL_OP_ANM_PLAY_POS:
    {
        Float3 pos = final_pos.pos + Float3(full->context.current_context->get_float_arg(2), full->context.current_context->get_float_arg(3), 0.0f);
        g_EffectManager->track(create_vm_front_at(g_EnemyManager->anim_statement_anms[full->context.current_context->get_int_arg(0)], full->context.current_context->get_int_arg(1),
                                                  &pos, full->context.current_context->get_float_arg(4)));
        break;
    }
    // anmPlay(file, script)
    case ECL_OP_ANM_PLAY:
        g_EffectManager->track(create_vm_front_at(g_EnemyManager->anim_statement_anms[full->context.current_context->get_int_arg(0)], full->context.current_context->get_int_arg(1),
                                                  &final_pos.pos, 0.0f));
        break;
    // anmPlayHigh(file, script)
    case ECL_OP_ANM_PLAY_HIGH:
        g_EffectManager->track(
            g_EnemyManager->anim_statement_anms[full->context.current_context->get_int_arg(0)]->create_vm(full->context.current_context->get_int_arg(1), &final_pos.pos, 0.0f, -1, 0));
        break;
    // anmPlayAbs(file, script)
    case ECL_OP_ANM_PLAY_ABS:
    {
        i32 file = full->context.current_context->get_int_arg(0);
        g_EffectManager->track(g_EnemyManager->anim_statement_anms[file]->create_vm_front(full->context.current_context->get_int_arg(1), -1, 0));
        break;
    }
    // anm339(file, script, frames): anmPlayAbs, then runs the VM for some
    // frames at once.
    case ECL_OP_ANM_339:
    {
        i32 file = full->context.current_context->get_int_arg(0);
        AnmId id = g_EnemyManager->anim_statement_anms[file]->create_vm_front(full->context.current_context->get_int_arg(1), -1, 0);
        g_EffectManager->track(id);
        AnmVm *vm = id.find_or_clear();
        for (i32 i = 0; i < full->context.current_context->get_int_arg(2); i++)
        {
            vm->run();
        }
        break;
    }
    // anmPlayRotate(file, script, rotation)
    case ECL_OP_ANM_PLAY_ROTATE:
    {
        AnmId id = create_vm_front_at(g_EnemyManager->anim_statement_anms[full->context.current_context->get_int_arg(0)], full->context.current_context->get_int_arg(1),
                                      &final_pos.pos, 0.0f);
        AnmVm *vm = id.find_or_clear();
        if (vm != NULL)
        {
            vm->entity_pos = final_pos.pos;
            vm->rotation.z = full->context.current_context->get_float_arg(2);
            vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
        }
        g_EffectManager->track(id);
        break;
    }
    // anm334(effect)
    case ECL_OP_ANM_334:
        g_EffectManager->create_tracked(full->context.current_context->get_int_arg(0), &final_pos.pos, 0);
        break;
    // anmSetMain(slot, script)
    case ECL_OP_ANM_SET_MAIN:
    {
        i32 slot = full->context.current_context->get_int_arg(0);
        i32 script = full->context.current_context->get_int_arg(1);
        delete_vm_and_clear(anm_ids[slot]);
        anm_ids[slot] =
            g_EnemyManager->anim_statement_anms[selected_anm_index]->create_vm_front(script, anm_layers + 7, 0);
        AnmVm *vm = anm_ids[slot].find_or_clear();
        if (slot == 0)
        {
            final_sprite_size.x = vm->scale.y * vm->sprite_size.y;
            final_sprite_size.y = vm->scale.x * vm->sprite_size.x;
        }
        if (flags_low & ENEMY_FLAG_INTANGIBLE)
        {
            anm_ids[slot].hide_tree();
        }
        if (slot == 0)
        {
            flags_low |= ENEMY_FLAG_DIRECTIONAL_ANM;
            anm_slot_0_script = anm_set_main = script;
            anm_direction = 0;
            anm_slot_0_anm_index = selected_anm_index;
        }
        break;
    }
    // anmReset: back to the main script in slot 0.
    case ECL_OP_ANM_RESET:
        delete_vm_and_clear(anm_ids[0]);
        anm_ids[0] =
            g_EnemyManager->anim_statement_anms[selected_anm_index]->create_vm_front(anm_set_main, anm_layers + 7, 0);
        flags_low &= ~ENEMY_FLAG_DIRECTIONAL_ANM;
        anm_slot_0_script = anm_set_main;
        anm_direction = 0;
        anm_slot_0_anm_index = selected_anm_index;
        break;
    // anmSelectedPlay(slot)
    case ECL_OP_ANM_SELECTED_PLAY:
    {
        i32 slot = full->context.current_context->get_int_arg(0);
        delete_vm_and_clear(anm_ids[slot]);
        anm_ids[slot] = g_EnemyManager->anim_statement_anms[selected_anm_index]->create_vm_front(anm_set_main + 5,
                                                                                                 anm_layers + 7, 0);
        AnmVm *vm = anm_ids[slot].find_or_clear();
        if (slot == 0)
        {
            final_sprite_size.x = vm->scale.y * vm->sprite_size.y;
            final_sprite_size.y = vm->scale.x * vm->sprite_size.x;
        }
        if (flags_low & ENEMY_FLAG_INTANGIBLE)
        {
            anm_ids[slot].hide_tree();
        }
        break;
    }
    // anm316(slot, n): script n + 5 of the main set, or the main script
    // itself for negative n.
    case ECL_OP_ANM_316:
    {
        i32 slot = full->context.current_context->get_int_arg(0);
        i32 n = full->context.current_context->get_int_arg(1);
        delete_vm_and_clear(anm_ids[slot]);
        anm_ids[slot] = g_EnemyManager->anim_statement_anms[selected_anm_index]->create_vm_front(
            n >= 0 ? anm_set_main + n + 5 : anm_set_main, anm_layers + 7, 0);
        AnmVm *vm = anm_ids[slot].find_or_clear();
        if (slot == 0)
        {
            final_sprite_size.x = vm->scale.y * vm->sprite_size.y;
            final_sprite_size.y = vm->scale.x * vm->sprite_size.x;
        }
        if (flags_low & ENEMY_FLAG_INTANGIBLE)
        {
            anm_ids[slot].hide_tree();
        }
        break;
    }
    // anmSwitch(slot, interrupt)
    case ECL_OP_ANM_SWITCH:
    {
        i32 slot = full->context.current_context->get_int_arg(0);
        AnmManager::interrupt_tree(anm_ids[slot], (i16)full->context.current_context->get_int_arg(1));
        break;
    }
    // moveReset: folds rel_pos into abs_pos and stops all movement.
    case ECL_OP_MOVE_RESET:
        abs_pos.pos += rel_pos.pos;
        rel_pos.pos = g_zero_vec;
        rel_pos.speed = 0.0f;
        abs_pos.speed = 0.0f;
        rel_pos.velocity = g_zero_vec;
        abs_pos.velocity = g_zero_vec;
        rel_pos.flags &= ~0xf;
        abs_pos.flags &= ~0xf;
        abs_pos_i.end_time = 0;
        rel_pos_i.end_time = 0;
        abs_angle_i.end_time = 0;
        rel_angle_i.end_time = 0;
        abs_speed_i.end_time = 0;
        rel_speed_i.end_time = 0;
        abs_radial_dist_i.end_time = 0;
        rel_radial_dist_i.end_time = 0;
        abs_ellipse_i.end_time = 0;
        rel_ellipse_i.end_time = 0;
        break;
    // movePos(x, y), movePosRel(x, y): -999999 keeps a coordinate.
    case ECL_OP_MOVE_POS:
    case ECL_OP_MOVE_POS_REL:
    {
        PosVel *pos_vel = instr->opcode == ECL_OP_MOVE_POS ? &abs_pos : &rel_pos;
        f32 x = full->context.current_context->get_float_arg(0);
        f32 y = full->context.current_context->get_float_arg(1);
        if (x > -999999.0)
        {
            pos_vel->pos.x = x;
        }
        if (y > -999999.0)
        {
            pos_vel->pos.y = y;
        }
        pos_vel->flags &= ~0xf;
        final_pos.pos = rel_pos.pos + abs_pos.pos;
        update_final_pos();
        break;
    }
    // movePos3d(x, y, z), movePos3dRel(x, y, z): moves by the offset.
    case ECL_OP_MOVE_POS_3D:
    case ECL_OP_MOVE_POS_3D_REL:
    {
        PosVel *pos_vel = instr->opcode == ECL_OP_MOVE_POS_3D ? &abs_pos : &rel_pos;
        f32 x = full->context.current_context->get_float_arg(0);
        f32 y = full->context.current_context->get_float_arg(1);
        f32 z = full->context.current_context->get_float_arg(2);
        pos_vel->pos.x += x;
        pos_vel->pos.y += y;
        pos_vel->pos.z += z;
        update_final_pos();
        break;
    }
    // movePosTime(time, mode, x, y) and movePosRelTime; 436/437 move by an
    // offset (mirrored with the enemy).
    case ECL_OP_MOVE_POS_TIME:
    case ECL_OP_MOVE_POS_REL_TIME:
    case ECL_OP_MOVE_POS_TIME_OFFSET:
    case ECL_OP_MOVE_POS_REL_TIME_OFFSET:
    {
        PosVel *pos_vel = instr->opcode == ECL_OP_MOVE_POS_TIME || instr->opcode == ECL_OP_MOVE_POS_TIME_OFFSET ? &abs_pos : &rel_pos;
        InterpStrange1 *interp = instr->opcode == ECL_OP_MOVE_POS_TIME || instr->opcode == ECL_OP_MOVE_POS_TIME_OFFSET ? &abs_pos_i : &rel_pos_i;
        f32 x = full->context.current_context->get_float_arg(2);
        f32 y = full->context.current_context->get_float_arg(3);
        if (full->context.current_context->get_int_arg(0) <= 0)
        {
            interp->end_time = 0;
            break;
        }
        if (instr->opcode == ECL_OP_MOVE_POS_TIME_OFFSET || instr->opcode == ECL_OP_MOVE_POS_REL_TIME_OFFSET)
        {
            x = flags_low & ENEMY_FLAG_MIRRORED ? pos_vel->pos.x - x : pos_vel->pos.x + x;
            y = pos_vel->pos.y + y;
        }
        interp->end_time = full->context.current_context->get_int_arg(0);
        interp->bezier_1 = g_zero_vec;
        interp->bezier_2 = g_zero_vec;
        interp->method_for_3d = full->context.current_context->get_int_arg(1);
        interp->flag_1d &= ~1;
        interp->initial = pos_vel->pos;
        interp->goal = Float3(x > -999999.0 ? x : pos_vel->pos.x, y > -999999.0 ? y : pos_vel->pos.y, 0.0f);
        interp->reset_timer();
        pos_vel->flags &= ~0xf;
        break;
    }
    // moveCurve(time, mode_x, mode_y, x, y) and its variants: separate
    // modes for x and y.
    case ECL_OP_MOVE_CURVE:
    case ECL_OP_MOVE_CURVE_REL:
    case ECL_OP_MOVE_CURVE_OFFSET:
    case ECL_OP_MOVE_CURVE_REL_OFFSET:
    {
        PosVel *pos_vel = instr->opcode == ECL_OP_MOVE_CURVE || instr->opcode == ECL_OP_MOVE_CURVE_OFFSET ? &abs_pos : &rel_pos;
        InterpStrange1 *interp = instr->opcode == ECL_OP_MOVE_CURVE || instr->opcode == ECL_OP_MOVE_CURVE_OFFSET ? &abs_pos_i : &rel_pos_i;
        f32 x = full->context.current_context->get_float_arg(3);
        f32 y = full->context.current_context->get_float_arg(4);
        if (full->context.current_context->get_int_arg(0) <= 0)
        {
            interp->end_time = 0;
            break;
        }
        if (instr->opcode == ECL_OP_MOVE_CURVE_OFFSET || instr->opcode == ECL_OP_MOVE_CURVE_REL_OFFSET)
        {
            x = flags_low & ENEMY_FLAG_MIRRORED ? pos_vel->pos.x - x : pos_vel->pos.x + x;
            y = pos_vel->pos.y + y;
        }
        interp->end_time = full->context.current_context->get_int_arg(0);
        interp->bezier_1 = g_zero_vec;
        interp->bezier_2 = g_zero_vec;
        interp->method_for_1d = full->context.current_context->get_int_arg(1);
        interp->flag_1d |= 1;
        interp->move_curve_mode = full->context.current_context->get_int_arg(2);
        interp->flag_1d |= 1;
        interp->initial = pos_vel->pos;
        interp->goal = Float3(x > -999999.0 ? x : pos_vel->pos.x, y > -999999.0 ? y : pos_vel->pos.y, 0.0f);
        interp->reset_timer();
        pos_vel->flags &= ~0xf;
        break;
    }
    // moveBezier(time, b1x, b1y, x, y, b2x, b2y), moveBezierRel
    case ECL_OP_MOVE_BEZIER:
    case ECL_OP_MOVE_BEZIER_REL:
    {
        PosVel *pos_vel = instr->opcode == ECL_OP_MOVE_BEZIER ? &abs_pos : &rel_pos;
        InterpStrange1 *interp = instr->opcode == ECL_OP_MOVE_BEZIER ? &abs_pos_i : &rel_pos_i;
        f32 x = full->context.current_context->get_float_arg(3);
        f32 y = full->context.current_context->get_float_arg(4);
        Float3 bezier_1;
        bezier_1.x = full->context.current_context->get_float_arg(1);
        bezier_1.y = full->context.current_context->get_float_arg(2);
        bezier_1.z = 0.0f;
        Float3 bezier_2;
        bezier_2.x = full->context.current_context->get_float_arg(5);
        bezier_2.y = full->context.current_context->get_float_arg(6);
        bezier_2.z = 0.0f;
        interp->end_time = full->context.current_context->get_int_arg(0);
        interp->bezier_1 = bezier_1;
        interp->bezier_2 = bezier_2;
        interp->method_for_3d = INTERP_BEZIER;
        interp->flag_1d &= ~1;
        interp->initial = pos_vel->pos;
        interp->goal = Float3(x > -999999.0 ? x : pos_vel->pos.x, y > -999999.0 ? y : pos_vel->pos.y, 0.0f);
        interp->reset_timer();
        pos_vel->flags &= ~0xf;
        break;
    }
    // moveVel(angle, speed) and the relative and
    // no-mirror (428, 430) forms. -999999 keeps the current value.
    case ECL_OP_MOVE_VEL:
    case ECL_OP_MOVE_VEL_REL:
    case ECL_OP_MOVE_VEL_NM:
    case ECL_OP_MOVE_VEL_REL_NM:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_VEL || instr->opcode == ECL_OP_MOVE_VEL_NM ? &abs_pos : &rel_pos;
        f32 angle = full->context.current_context->get_float_arg(0);
        f32 speed = full->context.current_context->get_float_arg(1);
        if (angle > -999999.0)
        {
            if ((flags_low & ENEMY_FLAG_MIRRORED) && (instr->opcode == ECL_OP_MOVE_VEL || instr->opcode == ECL_OP_MOVE_VEL_REL))
            {
                angle = mirror_angle(angle);
            }
            pv->set_angle(angle);
        }
        if (speed > -999999.0)
        {
            pv->speed = speed;
        }
        pv->flags &= ~0xf;
        break;
    }
    // moveVelTime(time, mode, angle, speed). With mode 7
    // (constant velocity) angle and speed are per-frame changes instead.
    case ECL_OP_MOVE_VEL_TIME:
    case ECL_OP_MOVE_VEL_REL_TIME:
    case ECL_OP_MOVE_VEL_TIME_NM:
    case ECL_OP_MOVE_VEL_REL_TIME_NM:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_VEL_TIME || instr->opcode == ECL_OP_MOVE_VEL_TIME_NM ? &abs_pos : &rel_pos;
        InterpFloat *angle_i = instr->opcode == ECL_OP_MOVE_VEL_TIME || instr->opcode == ECL_OP_MOVE_VEL_TIME_NM ? &abs_angle_i : &rel_angle_i;
        InterpFloat *speed_i = instr->opcode == ECL_OP_MOVE_VEL_TIME || instr->opcode == ECL_OP_MOVE_VEL_TIME_NM ? &abs_speed_i : &rel_speed_i;
        f32 angle = full->context.current_context->get_float_arg(2);
        f32 speed = full->context.current_context->get_float_arg(3);
        if (full->context.current_context->get_int_arg(0) <= 0)
        {
            angle_i->end_time = 0;
            speed_i->end_time = 0;
            break;
        }
        i32 mode = full->context.current_context->get_int_arg(1);
        if (mode != INTERP_CONSTANT_VELOCITY)
        {
            if (angle > -999999.0)
            {
                if ((flags_low & ENEMY_FLAG_MIRRORED) && (instr->opcode == ECL_OP_MOVE_VEL_TIME || instr->opcode == ECL_OP_MOVE_VEL_REL_TIME))
                {
                    angle = mirror_angle(angle);
                }
            }
            else
            {
                angle = pv->angle.value;
            }
            speed = speed > -999999.0 ? speed : pv->speed;
        }
        else
        {
            if (angle > -999999.0)
            {
                if ((flags_low & ENEMY_FLAG_MIRRORED) && (instr->opcode == ECL_OP_MOVE_VEL_TIME || instr->opcode == ECL_OP_MOVE_VEL_REL_TIME))
                {
                    angle = -angle;
                }
            }
            else
            {
                angle = 0.0f;
            }
            speed = speed > -999999.0 ? speed : 0.0f;
        }
        f32 cur_speed = pv->speed;
        f32 cur_angle = pv->angle.value;
        // Turn the short way round.
        if (zun_fabsf(cur_angle - angle) >= ZUN_PI)
        {
            if (angle > cur_angle)
            {
                cur_angle += ZUN_2PI;
            }
            else
            {
                angle += ZUN_2PI;
            }
        }
        angle_i->start(full->context.current_context->get_int_arg(0), mode, cur_angle, angle);
        speed_i->start(get_int_arg(0), mode, cur_speed, speed);
        pv->flags &= ~0xf;
        break;
    }
    // moveAngle(angle). Mirrored whenever the flag is set.
    case ECL_OP_MOVE_ANGLE:
    case ECL_OP_MOVE_ANGLE_REL:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_ANGLE ? &abs_pos : &rel_pos;
        f32 angle = get_float_arg(0);
        if (flags_low & ENEMY_FLAG_MIRRORED)
        {
            angle = mirror_angle(angle);
        }
        pv->set_angle(angle);
        pv->flags &= ~0xf;
        break;
    }
    // moveAngleTime(time, mode, angle).
    case ECL_OP_MOVE_ANGLE_TIME:
    case ECL_OP_MOVE_ANGLE_REL_TIME:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_ANGLE_TIME ? &abs_pos : &rel_pos;
        InterpFloat *angle_i = instr->opcode == ECL_OP_MOVE_ANGLE_TIME ? &abs_angle_i : &rel_angle_i;
        f32 angle = get_float_arg(2);
        if (get_int_arg(0) <= 0)
        {
            angle_i->end_time = 0;
            break;
        }
        i32 mode = get_int_arg(1);
        if (mode != INTERP_CONSTANT_VELOCITY)
        {
            if (angle > -999999.0)
            {
                if ((flags_low & ENEMY_FLAG_MIRRORED) && (instr->opcode == ECL_OP_MOVE_ANGLE_TIME || instr->opcode == ECL_OP_MOVE_ANGLE_REL_TIME))
                {
                    angle = mirror_angle(angle);
                }
            }
            else
            {
                angle = pv->angle.value;
            }
        }
        else
        {
            if (angle > -999999.0)
            {
                if ((flags_low & ENEMY_FLAG_MIRRORED) && (instr->opcode == ECL_OP_MOVE_ANGLE_TIME || instr->opcode == ECL_OP_MOVE_ANGLE_REL_TIME))
                {
                    angle = -angle;
                }
            }
            else
            {
                angle = 0.0f;
            }
        }
        f32 initial = pv->angle.value;
        // The goal as an offset from the current angle, the short way round.
        f32 delta;
        if (angle - initial > ZUN_PI)
        {
            delta = angle - (initial + ZUN_2PI);
        }
        else if (initial - angle > ZUN_PI)
        {
            delta = angle - (initial - ZUN_2PI);
        }
        else
        {
            delta = angle - initial;
        }
        angle_i->start(get_int_arg(0), mode, initial, delta + initial);
        pv->flags &= ~0xf;
        break;
    }
    // moveSpeed(speed).
    case ECL_OP_MOVE_SPEED:
    case ECL_OP_MOVE_SPEED_REL:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_SPEED ? &abs_pos : &rel_pos;
        f32 speed = get_float_arg(0);
        if (speed > -999999.0)
        {
            pv->speed = speed;
        }
        pv->flags &= ~0xf;
        break;
    }
    // moveSpeedTime(time, mode, speed).
    case ECL_OP_MOVE_SPEED_TIME:
    case ECL_OP_MOVE_SPEED_REL_TIME:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_SPEED_TIME ? &abs_pos : &rel_pos;
        InterpFloat *speed_i = instr->opcode == ECL_OP_MOVE_SPEED_TIME ? &abs_speed_i : &rel_speed_i;
        f32 speed = get_float_arg(2);
        f32 initial = pv->speed;
        if (get_int_arg(0) <= 0)
        {
            speed_i->end_time = 0;
            break;
        }
        i32 mode = get_int_arg(1);
        if (mode != INTERP_CONSTANT_VELOCITY)
        {
            speed = speed > -999999.0 ? speed : pv->speed;
        }
        else
        {
            speed = speed > -999999.0 ? speed : 0.0f;
        }
        speed_i->start(get_int_arg(0), mode, initial, speed);
        pv->flags &= ~0xf;
        break;
    }
    // moveCircle(angle, angular speed, radius, radial speed),
    // around the current position unless already circling.
    case ECL_OP_MOVE_CIRCLE:
    case ECL_OP_MOVE_CIRCLE_REL:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_CIRCLE ? &abs_pos : &rel_pos;
        f32 angle = get_float_arg(0);
        f32 speed = get_float_arg(1);
        f32 radius = get_float_arg(2);
        f32 radial_speed = get_float_arg(3);
        if ((pv->flags & POSVEL_MODE_MASK) != POSVEL_MODE_CIRCLE)
        {
            pv->velocity = pv->pos;
        }
        if (angle > -999999.0)
        {
            pv->set_angle(angle);
        }
        if (speed > -999999.0)
        {
            pv->speed = speed;
        }
        if (radius > -999999.0)
        {
            pv->radial_dist = radius;
        }
        if (radial_speed > -999999.0)
        {
            pv->radial_speed = radial_speed;
        }
        pv->flags = pv->flags & ~0xf | POSVEL_MODE_CIRCLE;
        pv->step_from_center();
        update_final_pos();
        break;
    }
    // moveCircleTime(time, mode, angular speed, radius, radial
    // speed).
    case ECL_OP_MOVE_CIRCLE_TIME:
    case ECL_OP_MOVE_CIRCLE_REL_TIME:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_CIRCLE_TIME ? &abs_pos : &rel_pos;
        InterpFloat *speed_i = instr->opcode == ECL_OP_MOVE_CIRCLE_TIME ? &abs_speed_i : &rel_speed_i;
        InterpFloat2 *radial_i = instr->opcode == ECL_OP_MOVE_CIRCLE_TIME ? &abs_radial_dist_i : &rel_radial_dist_i;
        f32 speed = get_float_arg(2);
        f32 radius = get_float_arg(3);
        f32 radial_speed = get_float_arg(4);
        speed = speed > -999999.0 ? speed : pv->speed;
        f32 initial_speed = pv->speed;
        f32 goal_radial_speed = radial_speed > -999999.0 ? radial_speed : pv->radial_speed;
        f32 goal_radius = radius > -999999.0 ? radius : pv->radial_dist;
        f32 initial_radius = pv->radial_dist;
        f32 initial_radial_speed = pv->radial_speed;
        i32 time = get_int_arg(0);
        i32 mode = get_int_arg(1);
        if (time <= 0)
        {
            speed_i->end_time = 0;
            radial_i->end_time = 0;
            break;
        }
        speed_i->start(time, mode, initial_speed, speed);
        radial_i->start(time, mode, initial_radius, initial_radial_speed, goal_radius, goal_radial_speed);
        pv->flags = pv->flags & ~0xf | POSVEL_MODE_CIRCLE;
        pv->step_from_center();
        update_final_pos();
        break;
    }
    // moveAdd (ExpHP): sets the center of circular and elliptic
    // movement.
    case ECL_OP_MOVE_ADD:
    case ECL_OP_MOVE_ADD_REL:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_ADD ? &abs_pos : &rel_pos;
        f32 x = get_float_arg(0);
        f32 y = get_float_arg(1);
        if (x > -999999.0)
        {
            pv->velocity.x = x;
        }
        if (y > -999999.0)
        {
            pv->velocity.y = y;
        }
        pv->step_from_center();
        update_final_pos();
    }
    // No break in the original: 418 and 419 go on into moveEllipse.
    // moveEllipse(angle, angular speed, radius, radial speed,
    // ellipse angle, ellipse ratio).
    case ECL_OP_MOVE_ELLIPSE:
    case ECL_OP_MOVE_ELLIPSE_REL:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_ELLIPSE ? &abs_pos : &rel_pos;
        f32 angle = get_float_arg(0);
        f32 speed = get_float_arg(1);
        f32 radius = get_float_arg(2);
        f32 radial_speed = get_float_arg(3);
        f32 ellipse_angle = get_float_arg(4);
        f32 ellipse_ratio = get_float_arg(5);
        if ((pv->flags & POSVEL_MODE_MASK) != POSVEL_MODE_CIRCLE)
        {
            pv->velocity = pv->pos;
        }
        if (angle > -999999.0)
        {
            pv->set_angle(angle);
        }
        if (speed > -999999.0)
        {
            pv->speed = speed;
        }
        if (radius > -999999.0)
        {
            pv->radial_dist = radius;
        }
        if (radial_speed > -999999.0)
        {
            pv->radial_speed = radial_speed;
        }
        if (ellipse_angle > -999999.0)
        {
            pv->set_ellipse_angle(ellipse_angle);
        }
        if (ellipse_ratio > -999999.0)
        {
            pv->ellipse_ratio = ellipse_ratio;
        }
        pv->flags = pv->flags & ~0xf | POSVEL_MODE_ELLIPSE;
        pv->step_from_center();
        update_final_pos();
        break;
    }
    // moveEllipseTime(time, mode, angular speed, radius, radial
    // speed, ellipse angle, ellipse ratio). Always restarts around the
    // current position.
    case ECL_OP_MOVE_ELLIPSE_TIME:
    case ECL_OP_MOVE_ELLIPSE_REL_TIME:
    {
        PosVel *pv = instr->opcode == ECL_OP_MOVE_ELLIPSE_TIME ? &abs_pos : &rel_pos;
        InterpFloat *speed_i = instr->opcode == ECL_OP_MOVE_ELLIPSE_TIME ? &abs_speed_i : &rel_speed_i;
        InterpFloat2 *radial_i = instr->opcode == ECL_OP_MOVE_ELLIPSE_TIME ? &abs_radial_dist_i : &rel_radial_dist_i;
        InterpFloat2 *ellipse_i = instr->opcode == ECL_OP_MOVE_ELLIPSE_TIME ? &abs_ellipse_i : &rel_ellipse_i;
        f32 speed = get_float_arg(2);
        f32 radius = get_float_arg(3);
        f32 radial_speed = get_float_arg(4);
        f32 ellipse_angle = get_float_arg(5);
        f32 ellipse_ratio = get_float_arg(6);
        speed = speed > -999999.0 ? speed : pv->speed;
        f32 initial_speed = pv->speed;
        f32 goal_radial_speed = radial_speed > -999999.0 ? radial_speed : pv->radial_speed;
        f32 goal_radius = radius > -999999.0 ? radius : pv->radial_dist;
        f32 initial_radius = pv->radial_dist;
        f32 initial_radial_speed = pv->radial_speed;
        f32 goal_ellipse_ratio = ellipse_ratio > -999999.0 ? ellipse_ratio : pv->ellipse_ratio;
        f32 goal_ellipse_angle = ellipse_angle > -999999.0 ? ellipse_angle : pv->ellipse_angle.value;
        f32 initial_ellipse_angle = pv->ellipse_angle.value;
        f32 initial_ellipse_ratio = pv->ellipse_ratio;
        i32 time = get_int_arg(0);
        i32 mode = get_int_arg(1);
        if (time <= 0)
        {
            speed_i->end_time = 0;
            radial_i->end_time = 0;
            ellipse_i->end_time = 0;
            break;
        }
        speed_i->start(time, mode, initial_speed, speed);
        radial_i->start(time, mode, initial_radius, initial_radial_speed, goal_radius, goal_radial_speed);
        ellipse_i->start(time, mode, initial_ellipse_angle, initial_ellipse_ratio, goal_ellipse_angle,
                         goal_ellipse_ratio);
        pv->velocity = pv->pos;
        pv->flags = pv->flags & ~0xf | POSVEL_MODE_ELLIPSE;
        pv->step_from_center();
        update_final_pos();
        break;
    }
    // moveBoss(): jumps to the boss.
    case ECL_OP_MOVE_BOSS:
        abs_pos.pos = g_EnemyManager->get_boss(0)->enemy.final_pos.pos;
        break;
    // moveBossRel().
    case ECL_OP_MOVE_BOSS_REL:
        rel_pos.pos = g_EnemyManager->get_boss(0)->enemy.final_pos.pos;
        break;
    // moveEnm(enemy id): jumps to another enemy.
    case ECL_OP_MOVE_ENM:
        abs_pos.pos = g_EnemyManager->find_enemy_by_id(get_int_arg(0))->enemy.final_pos.pos;
        break;
    // moveEnmRel(enemy id).
    case ECL_OP_MOVE_ENM_REL:
        rel_pos.pos = g_EnemyManager->find_enemy_by_id(get_int_arg(0))->enemy.final_pos.pos;
        break;
    // moveLimit(x, y, width, height): keeps final_pos inside the
    // rectangle (see update_final_pos).
    case ECL_OP_MOVE_LIMIT:
        flags_low |= ENEMY_FLAG_MOVE_LIMIT;
        move_limit_center.x = get_float_arg(0);
        move_limit_center.y = get_float_arg(1);
        move_limit_size.x = get_float_arg(2);
        move_limit_size.y = get_float_arg(3);
        break;
    // moveLimitReset().
    case ECL_OP_MOVE_LIMIT_RESET:
        flags_low &= ~ENEMY_FLAG_MOVE_LIMIT;
        break;
    // etProtectRange(radius), kept squared.
    case ECL_OP_ET_PROTECT_RANGE:
    {
        f32 range = get_float_arg(0);
        et_protect_range = range * range;
        break;
    }

    // moveRand(time, mode, speed) and moveRandRel: picks a random direction
    // that keeps the enemy inside its movement limit (move_limit_center,
    // move_limit_size), then starts the angle and speed interpolations.
    case ECL_OP_MOVE_RAND:
    case ECL_OP_MOVE_RAND_REL:
    {
        PosVel *pos = instr->opcode == ECL_OP_MOVE_RAND ? &abs_pos : &rel_pos;
        InterpFloat *angle_i = instr->opcode == ECL_OP_MOVE_RAND ? &abs_angle_i : &rel_angle_i;
        InterpFloat *speed_i = instr->opcode == ECL_OP_MOVE_RAND ? &abs_speed_i : &rel_speed_i;
        f32 angle;
        if (move_limit_center.x - move_limit_size.x / 4.0f > final_pos.pos.x)
        {
            angle = g_replay_safe_rng.randf_neg_1_to_1_times_pi() / 3.0f;
        }
        else if (final_pos.pos.x > move_limit_center.x + move_limit_size.x / 4.0f)
        {
            angle = normalize_angle(g_replay_safe_rng.randf_neg_1_to_1_times_pi() / 3.0f + ZUN_PI);
        }
        else if (g_Player->inner.pos.x > final_pos.pos.x)
        {
            // Mostly towards the player.
            angle = g_replay_safe_rng.randf_neg_1_to_1_times_pi() / 4.0f;
            if (g_replay_safe_rng.rand_u16_in_range(3) == 0)
            {
                angle += ZUN_PI;
            }
        }
        else
        {
            angle = g_replay_safe_rng.randf_neg_1_to_1_times_pi() / 4.0f;
            if (g_replay_safe_rng.rand_u16_in_range(3) != 0)
            {
                angle += ZUN_PI;
            }
        }
        if (move_limit_center.y - move_limit_size.y / 4.0f > final_pos.pos.y)
        {
            angle = zun_fabsf(angle);
        }
        else if (final_pos.pos.y > move_limit_center.y + move_limit_size.y / 4.0f)
        {
            angle = -zun_fabsf(angle);
        }
        // Keep away from straight up or down.
        if (zun_fabsf(angle + ZUN_PI / 2) < 0.05f)
        {
            angle = -ZUN_PI / 2 > angle ? -ZUN_PI / 2 - 0.05f : -ZUN_PI / 2 + 0.05f;
        }
        else if (zun_fabsf(angle - ZUN_PI / 2) < 0.05)
        {
            angle = ZUN_PI / 2 > angle ? ZUN_PI / 2 - 0.05f : ZUN_PI / 2 + 0.05f;
        }
        i32 mode = get_int_arg(1);
        f32 speed = get_float_arg(2);
        angle_i->end_time = get_int_arg(0);
        angle_i->method = mode;
        angle_i->bezier_1 = 0.0f;
        angle_i->bezier_2 = 0.0f;
        angle_i->initial = angle;
        angle_i->goal = angle;
        angle_i->reset();
        speed_i->end_time = get_int_arg(0);
        // ZUN's slip: the speed interpolator's method is never set.
        angle_i->method = mode;
        speed_i->bezier_1 = 0.0f;
        speed_i->bezier_2 = 0.0f;
        speed_i->initial = speed;
        speed_i->goal = 0.0f;
        speed_i->reset();
        pos->flags &= ~0xf;
        break;
    }
    // lifeMarker(index, life, color): a marker on the boss life bar.
    case ECL_OP_LIFE_MARKER:
        g_Gui->set_boss_life_marker(own_boss_id, get_int_arg(0), get_float_arg(1) / life.maximum, get_int_arg(2));
        break;
    // funcSet(index): a per-frame hook (g_ecl_func_sets).
    case ECL_OP_FUNC_SET:
        func_from_ecl_func_set = (void *)g_ecl_func_sets[get_int_arg(0)];
        is_func_set_2 = 0;
        break;
    // funcSet2(index): the same, flagged as the second kind.
    case ECL_OP_FUNC_SET2:
        func_from_ecl_func_set = (void *)g_ecl_func_sets[get_int_arg(0)];
        is_func_set_2 = 1;
        break;
    // flagExtDmg(index): a damage hook.
    case ECL_OP_FLAG_EXT_DMG:
        func_from_ecl_flag_ext_dmg = (void *)g_ecl_ext_damage_funcs[get_int_arg(0)];
        break;
    // unknown634(index)
    case ECL_OP_UNKNOWN_634:
        func_from_ecl_unknown_634 = g_ecl_unknown_634_funcs[get_int_arg(0)];
        break;
    // funcCall(index): runs a hook of g_ecl_func_sets once.
    case ECL_OP_FUNC_CALL:
        g_ecl_func_sets[get_int_arg(0)](this);
        break;
    // setHurtbox(w, h). The first call also counts the enemy as spawned in
    // the chapter.
    case ECL_OP_SET_HURTBOX:
        hurtbox_size.x = get_float_arg(0);
        hurtbox_size.y = get_float_arg(1);
        if (chapter_count == 0)
        {
            chapter_count = 1;
            g_Globals.enemies_spawned_in_chapter++;
        }
        break;
    // setHitbox(w, h)
    case ECL_OP_SET_HITBOX:
        hitbox_size.x = get_float_arg(0);
        hitbox_size.y = get_float_arg(1);
        break;
    // unknown569(count): how many enemies this one counts as in the chapter
    // statistics.
    case ECL_OP_CHAPTER_COUNT:
        if (chapter_count == 0)
        {
            chapter_count = get_int_arg(0);
            if (chapter_count > 0)
            {
                g_Globals.enemies_spawned_in_chapter += chapter_count;
            }
        }
        else if (chapter_count == 1)
        {
            chapter_count = get_int_arg(0);
            g_Globals.enemies_spawned_in_chapter += chapter_count - 1;
        }
        break;
    // unknown570(): counts the enemy as destroyed now.
    case ECL_OP_COUNT_DESTROYED:
        if (chapter_count != 0 && own_chapter == g_Globals.chapter)
        {
            g_Globals.enemies_destroyed_in_chapter += chapter_count;
            chapter_count = 0;
        }
        break;
    // unknown563(on)
    case ECL_OP_RECT_HITBOX:
        ((EnemyFlagsLow *)&flags_low)->rect_hitbox = get_int_arg(0);
        break;
    // unknown564(angle): sets rotation.
    case ECL_OP_SET_ROTATION:
        rotation = get_float_arg(0);
        break;
    // bombInvuln(multiplier): damage taken from bombs.
    case ECL_OP_BOMB_INVULN:
        bomb_damage_multiplier = get_float_arg(0);
        break;
    // flagSet(flags)
    case ECL_OP_FLAG_SET:
        flags_low |= get_int_arg(0);
        if (flags_low & ENEMY_FLAG_INTANGIBLE)
        {
            for (i32 i = 0; i < 16; i++)
            {
                anm_ids[i].hide_tree();
            }
        }
        break;
    // flagClear(flags)
    case ECL_OP_FLAG_CLEAR:
        flags_low &= ~get_int_arg(0);
        if (!(flags_low & ENEMY_FLAG_INTANGIBLE))
        {
            for (i32 i = 0; i < 16; i++)
            {
                anm_ids[i].show_tree();
            }
        }
        break;
    // dropClear(): forgets the extra drops except the season items.
    case ECL_OP_DROP_CLEAR:
    {
        i32 season_items = drops.extra_counts[15];
        memset(drops.extra_counts, 0, sizeof(drops.extra_counts));
        drops.extra_counts[15] = season_items;
        break;
    }
    // dropExtra(item_type, count)
    case ECL_OP_DROP_EXTRA:
    {
        i32 type = get_int_arg(0);
        drops.extra_counts[type - 1] = get_int_arg(1);
        break;
    }
    // dropArea(w, h)
    case ECL_OP_DROP_AREA:
    {
        f32 w = get_float_arg(0);
        f32 h = get_float_arg(1);
        drops.area.x = w;
        drops.area.y = h;
        break;
    }
    // dropItems(): drops everything now (not in spell practice). Season
    // items shrink with the bonus timer, as in EnemyInf::die.
    case ECL_OP_DROP_ITEMS:
        if (g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE)
        {
            break;
        }
        if (drop_season.bonus_timer.current <= 0)
        {
            drops.extra_counts[15] = drop_season.min_count;
        }
        else
        {
            drops.extra_counts[15] =
                (drops.extra_counts[15] - drop_season.min_count) * drop_season.bonus_timer.current /
                    drop_season.max_time +
                drop_season.min_count;
        }
    // unknown562(): drops everything now, in any mode.
    case ECL_OP_DROP_ITEMS_ANY_MODE:
        drops.eject_all_drops(&final_pos.pos);
        break;
    // dropMain(item_type)
    case ECL_OP_DROP_MAIN:
        drops.main_type = get_int_arg(0);
        break;
    // lifeSet(life)
    case ECL_OP_LIFE_SET:
    {
        i32 value = get_int_arg(0);
        life.current = value;
        life.maximum = value;
        life.remaining_for_cur_attack = value;
        if (flags_low & ENEMY_FLAG_BOSS)
        {
            ((EnemyFlagsLow *)&flags_low)->big_life = 1;
        }
        life.current_scaled_by_seven = value * 7;
        break;
    }
    // lifeNow(life)
    case ECL_OP_LIFE_NOW:
    {
        i32 value = get_int_arg(0);
        life.current = value;
        life.current_scaled_by_seven = value * 7;
        break;
    }
    // setBoss(id): makes this enemy boss id, or stops being a boss if id
    // is negative.
    case ECL_OP_SET_BOSS:
    {
        i32 boss_id = get_int_arg(0);
        g_EnemyManager->set_life_bar_hidden(0);
        if (boss_id < 0)
        {
            if (flags_low & ENEMY_FLAG_BOSS)
            {
                g_EnemyManager->inner.boss_ids[own_boss_id] = 0;
            }
            flags_low &= ~ENEMY_FLAG_BOSS;
        }
        else
        {
            flags_low |= ENEMY_FLAG_BOSS;
            g_EnemyManager->set_boss_id(boss_id, full);
            own_boss_id = boss_id;
        }
        break;
    }
    // setInvuln(time)
    case ECL_OP_SET_INVULN:
        set_invuln.set_value(get_int_arg(0));
        break;
    // unknown541(time): no hitbox for a while.
    case ECL_OP_NO_HITBOX_TIME:
        no_hitbox_dur.set_value(get_int_arg(0));
        break;
    // unknown551(value): kills the enemies whose kill_group is value.
    case ECL_OP_KILL_GROUP:
        EnemyManager::kill_all_in_group(get_int_arg(0));
        break;
    // unknown571(): kills every enemy without running set_death.
    case ECL_OP_KILL_ALL_NO_DEATH:
        EnemyManager::kill_all_no_set_death();
        break;
    // diffI(var, easy, normal, hard, lunatic)
    case ECL_OP_DIFF_I:
    {
        i32 easy = get_int_arg(1);
        i32 normal = get_int_arg(2);
        i32 hard = get_int_arg(3);
        i32 lunatic = get_int_arg(4);
        switch (g_Globals.difficulty)
        {
        case DIFFICULTY_EASY:
            *get_int_arg_ptr(0) = easy;
            break;
        case DIFFICULTY_NORMAL:
            *get_int_arg_ptr(0) = normal;
            break;
        case DIFFICULTY_HARD:
            *get_int_arg_ptr(0) = hard;
            break;
        case DIFFICULTY_LUNATIC:
        default:
            *get_int_arg_ptr(0) = lunatic;
            break;
        }
        break;
    }
    // diffF(var, easy, normal, hard, lunatic)
    case ECL_OP_DIFF_F:
    {
        f32 easy = get_float_arg(1);
        f32 normal = get_float_arg(2);
        f32 hard = get_float_arg(3);
        f32 lunatic = get_float_arg(4);
        switch (g_Globals.difficulty)
        {
        case DIFFICULTY_EASY:
            *get_float_arg_ptr(0) = easy;
            break;
        case DIFFICULTY_NORMAL:
            *get_float_arg_ptr(0) = normal;
            break;
        case DIFFICULTY_HARD:
            *get_float_arg_ptr(0) = hard;
            break;
        case DIFFICULTY_LUNATIC:
        default:
            *get_float_arg_ptr(0) = lunatic;
            break;
        }
        break;
    }
    // etNew(slot): resets a bullet shooter to one 2.0 speed bullet.
    case ECL_OP_ET_NEW:
    {
        i32 slot = get_int_arg(0);
        memset(&bullet_props[slot], 0, sizeof(EnemyBulletShooter));
        bullet_props[slot].ang_aim = 0.0f;
        bullet_props[slot].spd1 = 2.0f;
        bullet_props[slot].count = 1;
        bullet_props[slot].layers = 1;
        bullet_props[slot].shot_sfx = 21;
        bullet_props[slot].shot_transform_sfx = 38;
        bullet_props[slot].sfx_flags = 0x23;
        bullet_mgr_offsets[slot].xy.x = 0.0f;
        bullet_mgr_offsets[slot].xy.y = 0.0f;
        bullet_mgr_origins[slot].xy.x = 0.0f;
        bullet_mgr_origins[slot].xy.y = 0.0f;
        bullet_mgr_origins[slot].z = 0.0f;
        et_ex_index[slot] = 0;
        break;
    }
    // etCopy(dst, src)
    case ECL_OP_ET_COPY:
    {
        i32 dst = get_int_arg(0);
        i32 src = get_int_arg(1);
        bullet_props[dst] = bullet_props[src];
        bullet_mgr_offsets[dst] = bullet_mgr_offsets[src];
        bullet_mgr_origins[dst] = bullet_mgr_origins[src];
        break;
    }
    // etOn(slot): fires the shooter from the enemy plus the offset, or
    // from the absolute origin set by etOffsetAbs.
    case ECL_OP_ET_ON:
    {
        i32 slot = get_int_arg(0);
        if (bullet_mgr_origins[slot].z > 0.9f)
        {
            bullet_props[slot].pos =
                D3DXVECTOR3(bullet_mgr_origins[slot].xy.x + bullet_mgr_offsets[slot].xy.x,
                            bullet_mgr_origins[slot].xy.y + bullet_mgr_offsets[slot].xy.y, 0.0f);
        }
        else
        {
            bullet_props[slot].pos = D3DXVECTOR3(bullet_mgr_offsets[slot].xy.x + final_pos.pos.x,
                                                 bullet_mgr_offsets[slot].xy.y + final_pos.pos.y,
                                                 bullet_mgr_offsets[slot].z + final_pos.pos.z);
        }
        g_BulletManager->et_protect_range = et_protect_range;
        g_BulletManager->shoot_bullets(&bullet_props[slot]);
        g_BulletManager->et_protect_range = 0.0f;
        break;
    }
    // etSprite(slot, type, color)
    case ECL_OP_ET_SPRITE:
    {
        i32 slot = get_int_arg(0);
        bullet_props[slot].type = get_int_arg(1);
        bullet_props[slot].color = get_int_arg(2);
        break;
    }
    // etOffset(slot, x, y)
    case ECL_OP_ET_OFFSET:
    {
        i32 slot = get_int_arg(0);
        bullet_mgr_offsets[slot].xy.x = get_float_arg(1);
        bullet_mgr_offsets[slot].xy.y = get_float_arg(2);
        break;
    }
    // etOffsetRad(slot, angle, dist)
    case ECL_OP_ET_OFFSET_RAD:
    {
        i32 slot = get_int_arg(0);
        f32 angle = get_float_arg(1);
        Float3 offset;
        ecl_sincosmul(&offset, angle, get_float_arg(2));
        bullet_mgr_offsets[slot].xy.x = offset.x;
        bullet_mgr_offsets[slot].xy.y = offset.y;
        break;
    }
    // etDist(slot, dist)
    case ECL_OP_ET_DIST:
    {
        i32 slot = get_int_arg(0);
        bullet_props[slot].distance = get_float_arg(1);
        break;
    }
    // etOffsetAbs(slot, x, y): fire from an absolute position; x below
    // -990 goes back to firing from the enemy.
    case ECL_OP_ET_OFFSET_ABS:
    {
        i32 slot = get_int_arg(0);
        bullet_mgr_origins[slot].xy.x = get_float_arg(1);
        bullet_mgr_origins[slot].xy.y = get_float_arg(2);
        if (-990.0f > bullet_mgr_origins[slot].xy.x)
        {
            bullet_mgr_origins[slot].z = 0.0f;
        }
        else
        {
            bullet_mgr_origins[slot].z = 1.0f;
        }
        break;
    }
    // etAngle(slot, aim, spread)
    case ECL_OP_ET_ANGLE:
    {
        i32 slot = get_int_arg(0);
        bullet_props[slot].ang_aim = get_float_arg(1);
        bullet_props[slot].ang_bullet_dist = get_float_arg(2);
        break;
    }
    // etSpeed(slot, speed1, speed2)
    case ECL_OP_ET_SPEED:
    {
        i32 slot = get_int_arg(0);
        bullet_props[slot].spd1 = get_float_arg(1);
        bullet_props[slot].spd2 = get_float_arg(2);
        break;
    }
    // etCount(slot, count, layers)
    case ECL_OP_ET_COUNT:
    {
        i32 slot = get_int_arg(0);
        bullet_props[slot].count = get_int_arg(1);
        bullet_props[slot].layers = get_int_arg(2);
        break;
    }
    // etSpeedD(slot, speed1 x4, speed2 x4): etSpeed per difficulty.
    case ECL_OP_ET_SPEED_D:
    {
        i32 slot = get_int_arg(0);
        bullet_props[slot].spd1 = get_float_arg(ECL_DIFF_ARG(1));
        bullet_props[slot].spd2 = get_float_arg(ECL_DIFF_ARG(5));
        break;
    }
    // etCountD(slot, count x4, layers x4): etCount per difficulty.
    case ECL_OP_ET_COUNT_D:
    {
        i32 slot = get_int_arg(0);
        bullet_props[slot].count = (u16)get_int_arg(ECL_DIFF_ARG(1));
        bullet_props[slot].layers = (u16)get_int_arg(ECL_DIFF_ARG(5));
        break;
    }
    // etSpeedR3(slot, speed1, speed2 x3): etSpeed by rank (low, mid, high).
    case ECL_OP_ET_SPEED_R3:
    {
        i32 slot = get_int_arg(0);
        if (g_Globals.rank >= 512)
        {
            bullet_props[slot].spd1 = get_float_arg(5);
            bullet_props[slot].spd2 = get_float_arg(6);
        }
        else if (g_Globals.rank >= -512)
        {
            bullet_props[slot].spd1 = get_float_arg(3);
            bullet_props[slot].spd2 = get_float_arg(4);
        }
        else
        {
            bullet_props[slot].spd1 = get_float_arg(1);
            bullet_props[slot].spd2 = get_float_arg(2);
        }
        break;
    }
    // etSpeedR5(slot, speed1, speed2 x5): etSpeed by rank in five steps.
    case ECL_OP_ET_SPEED_R5:
    {
        i32 slot = get_int_arg(0);
        if (g_Globals.rank >= 600)
        {
            bullet_props[slot].spd1 = get_float_arg(9);
            bullet_props[slot].spd2 = get_float_arg(10);
        }
        else if (g_Globals.rank >= 200)
        {
            bullet_props[slot].spd1 = get_float_arg(7);
            bullet_props[slot].spd2 = get_float_arg(8);
        }
        else if (g_Globals.rank >= -200)
        {
            bullet_props[slot].spd1 = get_float_arg(5);
            bullet_props[slot].spd2 = get_float_arg(6);
        }
        else if (g_Globals.rank >= -600)
        {
            bullet_props[slot].spd1 = get_float_arg(3);
            bullet_props[slot].spd2 = get_float_arg(4);
        }
        else
        {
            bullet_props[slot].spd1 = get_float_arg(1);
            bullet_props[slot].spd2 = get_float_arg(2);
        }
        break;
    }
    // etSpeedR2(slot, speed1, speed2 at rank -1024, then at rank 1024):
    // interpolated by rank.
    case ECL_OP_ET_SPEED_R2:
    {
        i32 slot = get_int_arg(0);
        f32 low1 = get_float_arg(1);
        f32 low2 = get_float_arg(2);
        f32 high1 = get_float_arg(3);
        f32 high2 = get_float_arg(4);
        bullet_props[slot].spd1 = (high1 - low1) * (g_Globals.rank + 1024.0f) / 2048.0f + low1;
        bullet_props[slot].spd2 = (high2 - low2) * (g_Globals.rank + 1024.0f) / 2048.0f + low2;
        break;
    }
    // etCountR3(slot, count, layers x3)
    case ECL_OP_ET_COUNT_R3:
    {
        i32 slot = get_int_arg(0);
        if (g_Globals.rank >= 512)
        {
            bullet_props[slot].count = get_int_arg(5);
            bullet_props[slot].layers = get_int_arg(6);
        }
        else if (g_Globals.rank >= -512)
        {
            bullet_props[slot].count = get_int_arg(3);
            bullet_props[slot].layers = get_int_arg(4);
        }
        else
        {
            bullet_props[slot].count = get_int_arg(1);
            bullet_props[slot].layers = get_int_arg(2);
        }
        break;
    }
    // etCountR5(slot, count, layers x5)
    case ECL_OP_ET_COUNT_R5:
    {
        i32 slot = get_int_arg(0);
        if (g_Globals.rank >= 600)
        {
            bullet_props[slot].count = get_int_arg(9);
            bullet_props[slot].layers = get_int_arg(10);
        }
        else if (g_Globals.rank >= 200)
        {
            bullet_props[slot].count = get_int_arg(7);
            bullet_props[slot].layers = get_int_arg(8);
        }
        else if (g_Globals.rank >= -200)
        {
            bullet_props[slot].count = get_int_arg(5);
            bullet_props[slot].layers = get_int_arg(6);
        }
        else if (g_Globals.rank >= -600)
        {
            bullet_props[slot].count = get_int_arg(3);
            bullet_props[slot].layers = get_int_arg(4);
        }
        else
        {
            bullet_props[slot].count = get_int_arg(1);
            bullet_props[slot].layers = get_int_arg(2);
        }
        break;
    }
    // etCountR2(slot, count, layers at rank -1024, then at rank 1024)
    case ECL_OP_ET_COUNT_R2:
    {
        i32 slot = get_int_arg(0);
        i32 low_count = get_int_arg(1);
        i32 low_layers = get_int_arg(2);
        i32 high_count = get_int_arg(3);
        i32 high_layers = get_int_arg(4);
        bullet_props[slot].count = (high_count - low_count) * (g_Globals.rank + 1024) / 2048 + low_count;
        bullet_props[slot].layers = (high_layers - low_layers) * (g_Globals.rank + 1024) / 2048 + low_layers;
        break;
    }
    // etAim(slot, aim_type)
    case ECL_OP_ET_AIM:
    {
        i32 slot = get_int_arg(0);
        bullet_props[slot].aim_type = get_int_arg(1);
        break;
    }
    // etSound(slot, shot_sfx, transform_sfx)
    case ECL_OP_ET_SOUND:
    {
        i32 slot = get_int_arg(0);
        bullet_props[slot].shot_sfx = get_int_arg(1);
        bullet_props[slot].shot_transform_sfx = get_int_arg(2);
        break;
    }

    // etEx(et, ex_index, slot, type, a, b, r, s) and its variants: 610 adds
    // c, d and m, n; 611 and 612 take the next free ex slot instead of an
    // explicit index.
    case ECL_OP_ET_EX:
    case ECL_OP_ET_EX_FULL:
    case ECL_OP_ET_EX_NEXT:
    case ECL_OP_ET_EX_FULL_NEXT:
    {
        i32 idx = get_int_arg(0);
        if (idx >= 18 || idx < 0)
        {
            idx = 0;
        }
        i32 n = instr->opcode == ECL_OP_ET_EX_NEXT || instr->opcode == ECL_OP_ET_EX_FULL_NEXT ? 1 : 2;
        i32 ex_idx;
        if (instr->opcode == ECL_OP_ET_EX_NEXT || instr->opcode == ECL_OP_ET_EX_FULL_NEXT)
        {
            ex_idx = et_ex_index[idx];
        }
        else
        {
            ex_idx = get_int_arg(1);
        }
        bullet_props[idx].ex[ex_idx].slot = get_int_arg(n++);
        bullet_props[idx].ex[ex_idx].type = get_int_arg(n++);
        bullet_props[idx].ex[ex_idx].a = get_int_arg(n++);
        bullet_props[idx].ex[ex_idx].b = get_int_arg(n++);
        if (instr->opcode == ECL_OP_ET_EX_FULL || instr->opcode == ECL_OP_ET_EX_FULL_NEXT)
        {
            bullet_props[idx].ex[ex_idx].c = get_int_arg(n++);
            bullet_props[idx].ex[ex_idx].d = get_int_arg(n++);
        }
        bullet_props[idx].ex[ex_idx].r = get_float_arg(n++);
        bullet_props[idx].ex[ex_idx].s = get_float_arg(n++);
        if (instr->opcode == ECL_OP_ET_EX_FULL || instr->opcode == ECL_OP_ET_EX_FULL_NEXT)
        {
            bullet_props[idx].ex[ex_idx].m = get_float_arg(n++);
            bullet_props[idx].ex[ex_idx].n = get_float_arg(n++);
        }
        if (instr->opcode == ECL_OP_ET_EX_NEXT || instr->opcode == ECL_OP_ET_EX_FULL_NEXT)
        {
            et_ex_index[idx]++;
        }
        else
        {
            et_ex_index[idx] = ex_idx + 1;
        }
        break;
    }
    // etExSubtract(et): steps the next free ex slot back.
    case ECL_OP_ET_EX_SUBTRACT:
    {
        i32 idx = get_int_arg(0);
        et_ex_index[idx]--;
        if (et_ex_index[idx] < 0)
        {
            et_ex_index[idx] = 0;
        }
        break;
    }
    // etExSub(et, ex_index, sub): the subroutine an ex transform calls.
    case ECL_OP_ET_EX_SUB:
    {
        i32 idx = get_int_arg(0);
        bullet_props[idx].ex[get_int_arg(1)].string = (char *)&instr->args[3];
        break;
    }
    // laserNew(et, a, b, c, d).
    case ECL_OP_LASER_NEW:
    {
        i32 idx = get_int_arg(0);
        bullet_props[idx].pos.x = get_float_arg(1);
        bullet_props[idx].pos.y = get_float_arg(2);
        bullet_props[idx].pos.z = get_float_arg(3);
        bullet_props[idx].laser_new_arg_4 = get_float_arg(4);
        break;
    }
    // laserTiming(et, start, expand, duration, shrink, flags): the infinite
    // laser phases (LaserInfiniteInner::start_time and on).
    case ECL_OP_LASER_TIMING:
    {
        i32 idx = get_int_arg(0);
        bullet_props[idx].laser_timing[0] = get_int_arg(1);
        bullet_props[idx].laser_timing[1] = get_int_arg(2);
        bullet_props[idx].laser_timing[2] = get_int_arg(3);
        bullet_props[idx].laser_timing[3] = get_int_arg(4);
        bullet_props[idx].flags = get_int_arg(5);
        break;
    }
    // setNext(index, life, time, sub). In spell practice the boss instead
    // ends its card when the interrupt fires, and escapes on timeout.
    case ECL_OP_SET_NEXT:
    {
        const char *sub = (const char *)&instr->args[4];
        if (g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE && (flags_low & ENEMY_FLAG_BOSS))
        {
            if (own_boss_id == 0)
            {
                if (g_Globals.stage_num == 7 && g_Globals.chapter < 41)
                {
                    set_next(full, get_int_arg(0), 0, get_int_arg(2), "MBossDead");
                }
                else
                {
                    set_next(full, get_int_arg(0), 0, get_int_arg(2), "BossDead");
                }
            }
            else if (g_Globals.stage_num == 7 && g_Globals.chapter < 41)
            {
                set_next(full, get_int_arg(0), 0, get_int_arg(2), "MBossBDead");
            }
            else
            {
                set_next(full, get_int_arg(0), 0, get_int_arg(2), "BossDead");
            }
            full->set_timeout(get_int_arg(0), "BossEscape");
        }
        else
        {
            set_next(full, get_int_arg(0), get_int_arg(1), get_int_arg(2), sub);
        }
        break;
    }
    // setTimeout(index, sub).
    case ECL_OP_SET_TIMEOUT:
        full->set_timeout(get_int_arg(0), (const char *)&instr->args[2]);
        break;
    // setDeath(sub).
    case ECL_OP_SET_DEATH:
        strcpy(set_death, (const char *)&instr->args[1]);
        break;
    // etClearAll().
    case ECL_OP_ET_CLEAR_ALL:
        g_BulletManager->clear_all(0);
        g_LaserManager->clear_all(1, 0);
        break;
    // playSound(id).
    case ECL_OP_PLAY_SOUND:
        g_SoundManager.play_sound_at_position(get_int_arg(0), final_pos.pos.x);
        break;
    // setScreenShake(a, b, c).
    case ECL_OP_SET_SCREEN_SHAKE:
        ScreenEffect::create(SCREEN_EFFECT_SHAKE, get_int_arg(0), get_int_arg(1), get_int_arg(2), 0, 0x54);
        break;
    // dialogRead(script): also clears every bullet, laser and enemy.
    case ECL_OP_DIALOG_READ:
        g_Gui->start_dialogue(get_int_arg(0));
        g_BulletManager->clear_all(0);
        g_LaserManager->clear_all(0, 0);
        // fall through
    // enmKillAll().
    case ECL_OP_ENM_KILL_ALL:
        EnemyManager::kill_all();
        break;
    // dialogWait(): waits while dialogue runs and no enemy is expected.
    case ECL_OP_DIALOG_WAIT:
        if (g_Gui->msg != NULL && g_Gui->msg->ecl_resume_timer == 0)
        {
            return -1;
        }
        break;
    // Waits while any boss is alive.
    case ECL_OP_WAIT_BOSSES:
        if (g_EnemyManager->get_boss(0) != NULL || g_EnemyManager->get_boss(1) != NULL ||
            g_EnemyManager->get_boss(2) != NULL)
        {
            return -1;
        }
        break;
    // flagMirror(on).
    case ECL_OP_FLAG_MIRROR:
        ((EnemyFlagsLow *)&flags_low)->mirrored = get_int_arg(0);
        break;
    // spell(id, time_limit, boss_index, name) and its per-difficulty forms:
    // 537 to 539 add the difficulty (minus 0, 1 or 2) to the id. The name
    // is stored encrypted.
    case ECL_OP_SPELL:
    case ECL_OP_SPELL_528:
    case ECL_OP_SPELL_537:
    case ECL_OP_SPELL_538:
    case ECL_OP_SPELL_539:
    {
        char name[0x80];
        i32 len = instr->args[3].i;
        const char *src = (const char *)&instr->args[4];
        u8 key = 0x77;
        u8 step = 7;
        for (i32 i = 0; i < len; i++)
        {
            name[i] = src[i] ^ key;
            key += step;
            step += 0x10;
        }
        // The original inlines these three getters (0x473c90 through
        // full), unlike the cases around.
        i32 spell_id = full->context.current_context->get_int_arg(0);
        switch ((i16)instr->opcode)
        {
        case ECL_OP_SPELL_537:
            spell_id += g_Globals.difficulty;
            break;
        case ECL_OP_SPELL_538:
            spell_id += g_Globals.difficulty - 1;
            break;
        case ECL_OP_SPELL_539:
            spell_id += g_Globals.difficulty - 2;
            break;
        }
        g_Spellcard->start(spell_id, name, full->context.current_context->get_int_arg(1),
                           full->context.current_context->get_int_arg(2));
        life.is_spell |= ENEMY_LIFE_SPELL;
        life.current_scaled_by_seven = life.current * 7;
    }
        // fall through
    // timerReset().
    case ECL_OP_TIMER_RESET:
        time_in_ecl.set_value(0);
        break;
    // spellMode(on).
    case ECL_OP_SPELL_MODE:
        ((EnemyLifeSpellBits *)&life.is_spell)->active = get_int_arg(0);
        break;
    // spellEnd().
    case ECL_OP_SPELL_END:
        g_Spellcard->end();
        life.is_spell &= ~ENEMY_LIFE_SPELL;
        break;
    // spellTimeout(): no bonus decay.
    case ECL_OP_SPELL_TIMEOUT:
        g_Spellcard->flags |= SPELLCARD_NO_BONUS_DECAY;
        break;
    // unknown543(): removes the boss effect.
    case ECL_OP_HIDE_BOSS_EFFECT:
        g_Spellcard->flags |= SPELLCARD_FLAG_10;
        delete_vm_and_clear(g_Spellcard->boss_anm_id);
        break;
    // unknown567(on): the spell name at the bottom of the screen.
    case ECL_OP_SPELL_TEXT_AT_BOTTOM:
        ((SpellcardFlagBits *)&g_Spellcard->flags)->text_at_bottom = get_int_arg(0);
        break;
    // unknown544(on): ENEMY_FLAG_8000000.
    case ECL_OP_FLAG_8000000:
        ((EnemyFlagsLow *)&flags_low)->flag_8000000 = get_int_arg(0);
        break;
    // laserOn(et): a line laser from the shooter's settings.
    case ECL_OP_LASER_ON:
        ecl_laser_on(this);
        break;
    // laserStOn(et, a): an infinite laser.
    case ECL_OP_LASER_ST_ON:
        ecl_laser_st_on(this);
        break;
    // A beam laser (ExpHP: unknown713).
    case ECL_OP_LASER_BEAM_ON:
        ecl_laser_beam_on(this);
        break;
    // laserCuOn(et): a curvy laser.
    case ECL_OP_LASER_CU_ON:
        ecl_laser_cu_on(this);
        break;
    // laserStEnd(id): cancels every laser with the id.
    case ECL_OP_LASER_ST_END:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        while (laser != NULL)
        {
            laser->cancel(0, 0);
            laser->id = 0;
            laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        }
        break;
    }

    // laserOffset(id, x, y)
    case ECL_OP_LASER_OFFSET:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        Float3 offset;
        offset.x = get_float_arg(1);
        offset.y = get_float_arg(2);
        offset.z = 0.0f;
        if (laser != NULL)
        {
            laser->position = offset;
        }
        break;
    }
    // laserTrajectory(id, x, y): infinite lasers only.
    case ECL_OP_LASER_TRAJECTORY:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        Float3 trajectory;
        trajectory.x = get_float_arg(1);
        trajectory.y = get_float_arg(2);
        trajectory.z = 0.0f;
        if (laser != NULL)
        {
            ((LaserInfiniteInf *)laser)->inner.velocity = trajectory;
        }
        break;
    }
    // laserStLength(id, length)
    case ECL_OP_LASER_ST_LENGTH:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            laser->length = get_float_arg(1);
        }
        break;
    }
    // laserStWidth(id, width)
    case ECL_OP_LASER_ST_WIDTH:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            laser->width = get_float_arg(1);
        }
        break;
    }
    // laserStAngle(id, angle)
    case ECL_OP_LASER_ST_ANGLE:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            laser->angle = get_float_arg(1);
        }
        break;
    }
    // laserStRotation(id, rotation): infinite lasers only.
    case ECL_OP_LASER_ST_ROTATION:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            ((LaserInfiniteInf *)laser)->inner.laser_st_rotation = get_float_arg(1);
        }
        break;
    }
    // unknown714(id, value): laser method_8.
    case ECL_OP_LASER_714:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            laser->method_8(get_int_arg(1));
        }
        break;
    }
    // etCancel(radius): cancels bullets and lasers around the enemy into
    // items.
    case ECL_OP_ET_CANCEL:
    {
        f32 radius = get_float_arg(0);
        g_BulletManager->cancel_radius(&final_pos.pos, radius, 1);
        g_LaserManager->cancel_in_radius(&final_pos.pos, radius, 1, 1);
        break;
    }
    // hitboxRect(w, h): a rectangular bullet-cancelling hitbox, rotated like
    // the enemy's main sprite.
    case ECL_OP_HITBOX_RECT:
    {
        Float3 size;
        size.x = get_float_arg(0);
        size.y = get_float_arg(1);
        g_BulletManager->cancel_rectangle_as_bomb(&final_pos.pos, &size, anm_ids[0].find_or_clear()->rotation.z, 1);
        break;
    }
    // etClear(radius): the same without items.
    case ECL_OP_ET_CLEAR:
    {
        f32 radius = get_float_arg(0);
        g_BulletManager->cancel_radius(&final_pos.pos, radius, 0);
        g_LaserManager->cancel_in_radius(&final_pos.pos, radius, 0, 1);
        break;
    }
    // etCancel2(radius): etCancel that also takes protected bullets.
    case ECL_OP_ET_CANCEL2:
    {
        f32 radius = get_float_arg(0);
        g_BulletManager->cancel_radius_as_bomb(&final_pos.pos, radius, 1);
        g_LaserManager->cancel_in_radius(&final_pos.pos, radius, 1, 1);
        break;
    }
    // etClear2(radius)
    case ECL_OP_ET_CLEAR2:
    {
        f32 radius = get_float_arg(0);
        g_BulletManager->cancel_radius_as_bomb(&final_pos.pos, radius, 0);
        g_LaserManager->cancel_in_radius(&final_pos.pos, radius, 0, 1);
        break;
    }
    // setChapter(chapter)
    case ECL_OP_SET_CHAPTER:
    {
        i32 chapter = get_int_arg(0);
        g_Globals.chapter = chapter;
        g_GameThread->chapter = chapter;
        own_chapter = chapter;
        chapter_count = 0;
        break;
    }
    // rankF3(var, a, b, c): only two tiers survive in the binary.
    case ECL_OP_RANK_F3:
        if (g_Globals.rank >= 512)
        {
            *get_float_arg_ptr(0) = get_float_arg(2);
        }
        else
        {
            *get_float_arg_ptr(0) = get_float_arg(0);
        }
        break;
    // rankF5(var, a, b, c, d, e)
    case ECL_OP_RANK_F5:
        if (g_Globals.rank >= 600)
        {
            *get_float_arg_ptr(0) = get_float_arg(4);
        }
        else if (g_Globals.rank >= 200)
        {
            *get_float_arg_ptr(0) = get_float_arg(3);
        }
        else if (g_Globals.rank >= -200)
        {
            *get_float_arg_ptr(0) = get_float_arg(2);
        }
        else if (g_Globals.rank >= -400)
        {
            *get_float_arg_ptr(0) = get_float_arg(1);
        }
        else
        {
            *get_float_arg_ptr(0) = get_float_arg(0);
        }
        break;
    // rankF2(var, low, high): low at rank -1024, high at rank 1024.
    case ECL_OP_RANK_F2:
    {
        f32 low = get_float_arg(1);
        f32 high = get_float_arg(2);
        *get_float_arg_ptr(0) = (high - low) * (g_Globals.rank + 1024.0f) / 2048.0f + low;
        break;
    }
    // rankI3(var, a, b, c)
    case ECL_OP_RANK_I3:
        if (g_Globals.rank >= 512)
        {
            *get_int_arg_ptr(0) = get_int_arg(2);
        }
        else
        {
            *get_int_arg_ptr(0) = get_int_arg(0);
        }
        break;
    // rankI5(var, a, b, c, d, e)
    case ECL_OP_RANK_I5:
        if (g_Globals.rank >= 600)
        {
            *get_int_arg_ptr(0) = get_int_arg(4);
        }
        else if (g_Globals.rank >= 200)
        {
            *get_int_arg_ptr(0) = get_int_arg(3);
        }
        else if (g_Globals.rank >= -200)
        {
            *get_int_arg_ptr(0) = get_int_arg(2);
        }
        else if (g_Globals.rank >= -400)
        {
            *get_int_arg_ptr(0) = get_int_arg(1);
        }
        else
        {
            *get_int_arg_ptr(0) = get_int_arg(0);
        }
        break;
    // rankI2(var, low, high)
    case ECL_OP_RANK_I2:
    {
        i32 low = get_int_arg(1);
        i32 high = get_int_arg(2);
        *get_int_arg_ptr(0) = (high - low) * (g_Globals.rank + 1024) / 2048 + low;
        break;
    }
    // stars(count): the boss's star count on the HUD.
    case ECL_OP_STARS:
        g_Gui->boss_star_count = get_int_arg(0);
        break;
    // reset: cancels every laser.
    case ECL_OP_RESET:
        g_LaserManager->cancel_all();
        break;
    // bombShield(on, anm_script)
    case ECL_OP_BOMB_SHIELD:
        ((EnemyFlagsLow *)&flags_low)->bombshield = get_int_arg(0);
        bombshield_on_anm_main = get_int_arg(1);
        flags_low &= ~(ENEMY_FLAG_BOMBSHIELD_UP | ENEMY_FLAG_NO_HURTBOX);
        bombshield_off_anm_main = anm_set_main;
        break;
    // unknown559(limit): the enemy limit.
    case ECL_OP_ENEMY_LIMIT:
        g_EnemyManager->inner.enemy_limit = get_int_arg(0);
        break;
    // gameSpeed(speed)
    case ECL_OP_GAME_SPEED:
        g_game_speed = get_float_arg(0);
        break;
    // angleToPlayer(var, x, y)
    case ECL_OP_ANGLE_TO_PLAYER:
        ecl_angle_to_player(this);
        break;
    // diffWait(easy, normal, hard, lunatic): waits the given number of
    // frames for the current difficulty.
    case ECL_OP_DIFF_WAIT:
    {
        i32 easy = get_int_arg(0);
        i32 normal = get_int_arg(1);
        i32 hard = get_int_arg(2);
        i32 lunatic = get_int_arg(3);
        i32 wait;
        if (g_Globals.difficulty == DIFFICULTY_EASY)
        {
            wait = easy;
        }
        else if (g_Globals.difficulty == DIFFICULTY_NORMAL)
        {
            wait = normal;
        }
        else if (g_Globals.difficulty == DIFFICULTY_HARD)
        {
            wait = hard;
        }
        else
        {
            wait = lunatic;
        }
        full->context.current_context->time -= wait;
        break;
    }
    // fog(radius, color): replaces the enemy's fog; radius 0 removes it.
    case ECL_OP_FOG:
        if (fog.fog_ptr != NULL)
        {
            delete (Fog *)fog.fog_ptr;
        }
        fog.fog_ptr = NULL;
        fog.fog_radius = get_float_arg(0);
        fog.cur_radius = 16.0f;
        fog.fog_color = get_int_arg(1);
        *(ZunAngle *)&fog.wave_angle_x = 0.0f;
        *(ZunAngle *)&fog.wave_angle_y = 0.0f;
        if (fog.fog_radius > 0.0f)
        {
            fog.fog_ptr = new_enemy_fog();
        }
        break;
    // callSTD(label): jumps the stage script.
    case ECL_OP_CALL_STD:
        g_Stage->jump_to_label(get_int_arg(0));
        break;
    // unknown557(time, method, color, begin, end): interpolates the stage
    // fog (camera sky) towards the given distances and color.
    case ECL_OP_STAGE_FOG:
    {
        i32 time = get_int_arg(0);
        i32 method = get_int_arg(1);
        ZunColor color;
        color.d3d = get_int_arg(2);
        CameraSky sky(get_float_arg(3), get_float_arg(4), color.b, color.g, color.r, color.a);
        g_Stage->inner.set_sky_interp(time, method, &sky);
        break;
    }
    // unknown549(flag)
    case ECL_OP_MAGENTA_FLASH:
        ((EnemyFlagsLow *)&flags_low)->magenta_flash = get_int_arg(0);
        break;
    // unknown550(value)
    case ECL_OP_SET_KILL_GROUP:
        kill_group = get_int_arg(0);
        break;
    // enmAlive(var, id)
    case ECL_OP_ENM_ALIVE:
        *get_int_arg_ptr(0) = g_EnemyManager->is_enemy_alive(get_int_arg(1));
        break;
    // enmPos(var_x, var_y, id)
    case ECL_OP_ENM_POS:
    {
        EnemyInf *enemy = g_EnemyManager->find_enemy_by_id(get_int_arg(2));
        *get_float_arg_ptr(0) = enemy->enemy.final_pos.pos.x;
        *get_float_arg_ptr(1) = enemy->enemy.final_pos.pos.y;
        break;
    }
    // unknown802(index): makes the other bosses jump to their interrupt
    // index's subroutine, if it has a life threshold.
    case ECL_OP_BOSSES_INTERRUPT:
    {
        i32 index = get_int_arg(0);
        for (i32 i = 0; i < 3; i++)
        {
            EnemyInf *boss = g_EnemyManager->get_boss(i);
            if (i != own_boss_id && boss != NULL && boss->enemy.interrupts[index].life >= 0)
            {
                char *sub = boss->enemy.interrupts[index].sub_for_set_next;
                if (sub != NULL)
                {
                    boss->free_all_async();
                    boss->reset_run_context();
                    boss->load_sub_by_name(sub);
                }
            }
        }
        break;
    }
    // enmCall(id, sub): restarts another enemy at the named subroutine.
    case ECL_OP_ENM_CALL:
    {
        EnemyInf *enemy = g_EnemyManager->find_enemy_by_id(get_int_arg(0));
        if (enemy != NULL)
        {
            enemy->free_all_async();
            enemy->reset_run_context();
            enemy->load_sub_by_name((const char *)&instr->args[2]);
        }
        break;
    }
    // enm324(var_x, var_y, id): enmPos that falls back to this enemy.
    case ECL_OP_ENM_POS_OR_SELF:
    {
        EnemyInf *enemy = g_EnemyManager->find_enemy_by_id(get_int_arg(2));
        *get_float_arg_ptr(0) = enemy != NULL ? enemy->enemy.final_pos.pos.x : final_pos.pos.x;
        *get_float_arg_ptr(1) = enemy != NULL ? enemy->enemy.final_pos.pos.y : final_pos.pos.y;
        break;
    }
    // unknown560(x, y)
    case ECL_OP_UNKNOWN_560:
        g_BulletManager->ecl_unknown_560.x = get_float_arg(0);
        g_BulletManager->ecl_unknown_560.y = get_float_arg(1);
        break;
    // spec0(max_time, count, min_count): the season item drop.
    case ECL_OP_SPEC0:
        drop_season.max_time = get_int_arg(0);
        drop_season.bonus_timer.set_value(drop_season.max_time);
        drops.extra_counts[15] = get_int_arg(1);
        drop_season.min_count = get_int_arg(2);
        break;
    // spec1(damage_per_drop)
    case ECL_OP_SPEC1:
        drop_season.damage_per_season_drop = get_int_arg(0);
        drop_season.damage_accounted_for_season_drops = life.total_damage_including_ignored;
        break;
    // anm340(id): sets flag 0x2000000 on another enemy.
    case ECL_OP_ENM_DELETE:
    {
        EnemyInf *enemy = g_EnemyManager->find_enemy_by_id(get_int_arg(0));
        if (enemy != NULL)
        {
            enemy->enemy.flags_low |= ENEMY_FLAG_DELETE;
        }
        break;
    }

    }
    return 0;
}
