#include <math.h>
#include <stddef.h>
#include <string.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "AsciiManager.h"
#include "EffectManager.h"
#include "GameThread.h"
#include "Rng.h"
#include "Supervisor.h"

static_assert(offsetof(AnmVm, rotation_related) == 0x5f0, "AnmVm layout");
static_assert(sizeof(AnmVm) == 0x5fc, "AnmVm layout");
static_assert(sizeof(InterpInt3) == 0x58, "InterpInt3 layout");
static_assert(sizeof(InterpAngle) == 0x30, "InterpAngle layout");
static_assert(offsetof(AnmVm, pos_i) == 0x8c, "AnmVm layout");
static_assert(offsetof(AnmVm, rotate_i) == 0x16c, "AnmVm layout");
static_assert(offsetof(AnmVm, rotate_2d_i) == 0x1c4, "AnmVm layout");
static_assert(offsetof(AnmVm, u_vel_i) == 0x348, "AnmVm layout");
static_assert(offsetof(AnmVm, uv_quad_of_sprite) == 0x3a8, "AnmVm layout");
static_assert(offsetof(AnmVm, int_vars) == 0x4a0, "AnmVm layout");
static_assert(offsetof(AnmVm, pos_2) == 0x4e0, "AnmVm layout");
static_assert(offsetof(AnmVm, index_of_sprite_mapping_func) == 0x5dc, "AnmVm layout");
static_assert(offsetof(AsciiInf, ascii_anm) == 0x19240, "AsciiInf layout");
static_assert(offsetof(Supervisor, cameras) + 3 * sizeof(Camera) + offsetof(Camera, unk_104) == 0x6c0, "Supervisor layout");

// FUNCTION: TH16 0x45f2d0
HARNESS_CALLED f32 AnmVm::get_float_var(f32 value)
{
    switch ((i32)value)
    {
    case ANM_VAR_I0:
        return int_vars[0];
    case ANM_VAR_I1:
        return int_vars[1];
    case ANM_VAR_I2:
        return int_vars[2];
    case ANM_VAR_I3:
        return int_vars[3];
    case ANM_VAR_F0:
        return float_vars[0];
    case ANM_VAR_F1:
        return float_vars[1];
    case ANM_VAR_F2:
        return float_vars[2];
    case ANM_VAR_F3:
        return float_vars[3];
    case ANM_VAR_F4:
        return script_vars_33_34_35.x;
    case ANM_VAR_F5:
        return script_vars_33_34_35.y;
    case ANM_VAR_F6:
        return script_vars_33_34_35.z;
    case ANM_VAR_I4:
        return script_var_8;
    case ANM_VAR_I5:
        return script_var_9;
    case ANM_VAR_RANDF_UNSAFE:
        return g_replay_unsafe_rng.randf_0_to(rand_scale_one);
    case ANM_VAR_RANDF2_UNSAFE:
        return g_replay_unsafe_rng.randf_neg_to(rand_scale_one);
    case ANM_VAR_RANDRAD_UNSAFE:
        return g_replay_unsafe_rng.randf_neg_to(rand_scale_pi);
    case ANM_VAR_POS_X:
        return pos.x;
    case ANM_VAR_POS_Y:
        return pos.y;
    case ANM_VAR_POS_Z:
        return pos.z;
    case ANM_VAR_ROT_X:
        return rotation.x;
    case ANM_VAR_ROT_Y:
        return rotation.y;
    case ANM_VAR_ROT_Z:
        return rotation.z;
    case ANM_VAR_TOTAL_ROT_Z:
        return get_total_rotation()->z;
    case ANM_VAR_CAMERA_X:
        return g_Supervisor.cameras[3].position.x + g_Supervisor.cameras[3].rocking_vector_1.x;
    case ANM_VAR_CAMERA_Y:
        return g_Supervisor.cameras[3].position.y + g_Supervisor.cameras[3].rocking_vector_1.y;
    case ANM_VAR_CAMERA_Z:
        return g_Supervisor.cameras[3].position.z + g_Supervisor.cameras[3].rocking_vector_1.z;
    case ANM_VAR_CAMERA_FACING_X:
        return g_Supervisor.cameras[3].facing_normalized.x;
    case ANM_VAR_CAMERA_FACING_Y:
        return g_Supervisor.cameras[3].facing_normalized.y;
    case ANM_VAR_CAMERA_FACING_Z:
        return g_Supervisor.cameras[3].facing_normalized.z;
    case ANM_VAR_RAND_SCALE_ONE:
        return rand_scale_one;
    case ANM_VAR_RAND_SCALE_PI:
        return rand_scale_pi;
    case ANM_VAR_NUM_CYCLES:
        return num_cycles_in_texture;
    case ANM_VAR_RAND_UNSAFE:
        return g_replay_unsafe_rng.rand_u32();
    case ANM_VAR_RANDF_SAFE:
        return g_replay_safe_rng.randf_0_to(rand_scale_one);
    case ANM_VAR_RANDF2_SAFE:
        return g_replay_safe_rng.randf_neg_to(rand_scale_one);
    case ANM_VAR_RANDRAD_SAFE:
        return g_replay_safe_rng.randf_neg_to(rand_scale_pi);
    }
    return value;
}

// FUNCTION: TH16 0x45f610
HARNESS_CALLED i32 AnmVm::get_int_var(i32 value)
{
    switch (value)
    {
    case ANM_VAR_I0:
        return int_vars[0];
    case ANM_VAR_I1:
        return int_vars[1];
    case ANM_VAR_I2:
        return int_vars[2];
    case ANM_VAR_I3:
        return int_vars[3];
    case ANM_VAR_F0:
        return float_vars[0];
    case ANM_VAR_F1:
        return float_vars[1];
    case ANM_VAR_F2:
        return float_vars[2];
    case ANM_VAR_F3:
        return float_vars[3];
    case ANM_VAR_F4:
        return script_vars_33_34_35.x;
    case ANM_VAR_F5:
        return script_vars_33_34_35.y;
    case ANM_VAR_F6:
        return script_vars_33_34_35.z;
    case ANM_VAR_I4:
        return script_var_8;
    case ANM_VAR_I5:
        return script_var_9;
    case ANM_VAR_RAND_SCALE_ONE:
        return rand_scale_one;
    case ANM_VAR_RAND_SCALE_PI:
        return rand_scale_pi;
    case ANM_VAR_NUM_CYCLES:
        return num_cycles_in_texture;
    case ANM_VAR_RAND_UNSAFE:
        return g_replay_unsafe_rng.rand_u32_in_range(num_cycles_in_texture);
    }
    return value;
}

// FUNCTION: TH16 0x45f780
HARNESS_CALLED f32 *AnmVm::get_float_var_ptr(f32 *value)
{
    switch ((i32)*value)
    {
    case ANM_VAR_F0:
        return &float_vars[0];
    case ANM_VAR_F1:
        return &float_vars[1];
    case ANM_VAR_F2:
        return &float_vars[2];
    case ANM_VAR_F3:
        return &float_vars[3];
    case ANM_VAR_POS_X:
        return &pos.x;
    case ANM_VAR_POS_Y:
        return &pos.y;
    case ANM_VAR_POS_Z:
        return &pos.z;
    case ANM_VAR_ROT_X:
        return &rotation.x;
    case ANM_VAR_ROT_Y:
        return &rotation.y;
    case ANM_VAR_ROT_Z:
        return &rotation.z;
    case ANM_VAR_F4:
        return &script_vars_33_34_35.x;
    case ANM_VAR_F5:
        return &script_vars_33_34_35.y;
    case ANM_VAR_F6:
        return &script_vars_33_34_35.z;
    case ANM_VAR_RAND_SCALE_ONE:
        return &rand_scale_one;
    case ANM_VAR_RAND_SCALE_PI:
        return &rand_scale_pi;
    }
    return value;
}

// FUNCTION: TH16 0x45f890
HARNESS_CALLED i32 *AnmVm::get_int_var_ptr(i32 *value)
{
    switch (*value)
    {
    case ANM_VAR_I0:
        return &int_vars[0];
    case ANM_VAR_I1:
        return &int_vars[1];
    case ANM_VAR_I2:
        return &int_vars[2];
    case ANM_VAR_I3:
        return &int_vars[3];
    case ANM_VAR_I4:
        return &script_var_8;
    case ANM_VAR_I5:
        return &script_var_9;
    case ANM_VAR_NUM_CYCLES:
        return &num_cycles_in_texture;
    }
    return value;
}

// FUNCTION: TH16 0x45f940
void LTCG_FASTCALL divide_vec2_by_640_480(Float2 *out, Float2 *in)
{
    out->x = in->x / 640.0f;
    out->y = in->y / 480.0f;
    if (out->x < 0.0f)
    {
        out->x = 0.0f;
    }
    if (out->y < 0.0f)
    {
        out->y = 0.0f;
    }
}

// This file's copies of ZunMath.h's sincosmul (TH16 keeps one per object
// file): the first writes the two results through separate pointers.
// FUNCTION: TH16 0x464930
static void __fastcall anm_sincosmul_xy(f32 *x, f32 *y, f32 angle, f32 radius)
{
    __asm {
        mov eax, x
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        mov eax, y
        fstp [eax]
    }
}

// FUNCTION: TH16 0x464d60
static void __fastcall anm_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

// ANM instruction arguments: argument n is a constant unless bit n of
// var_mask says it names a script variable.
#define ANM_IS_VAR(n) (ins->var_mask & (1 << (n)))
#define ANM_INT(n) (ANM_IS_VAR(n) ? get_int_var(ins->args[n].i) : ins->args[n].i)
#define ANM_FLOAT(n) (ANM_IS_VAR(n) ? get_float_var(ins->args[n].f) : ins->args[n].f)
#define ANM_INT_PTR(n) (ANM_IS_VAR(n) ? get_int_var_ptr(&ins->args[n].i) : &ins->args[n].i)
#define ANM_FLOAT_PTR(n) (ANM_IS_VAR(n) ? get_float_var_ptr(&ins->args[n].f) : &ins->args[n].f)

#define ANM_FLAGS_LO ((AnmVmFlagsLoFields *)&flags_lo)
#define ANM_FLAGS_HI ((AnmVmFlagsHiFields *)&flags_hi)

// The color arguments of instructions 408 and 413 (and the current color),
// as set_rgb1_time and set_rgb2_time take them. Alpha is left unset.
static inline ZunColor anm_rgb(i32 r, i32 g, i32 b)
{
    ZunColor c;
    c.b = b;
    c.g = g;
    c.r = r;
    return c;
}

// 0x469e20. Sets up render mode 10 (ANM instruction 302): extra data and
// the on_tick and on_draw callbacks 4 and 6.
int __fastcall anm_effect_4_init(AnmVm *vm);

// Runs the script up to the current time and steps everything that
// changes on its own. 1 once the VM should be deleted.
// FUNCTION: TH16 0x45f980
i32 AnmVm::run()
{
    AnmRawInstr *ins;
    f32 saved_game_speed = g_game_speed;
    if (flags_hi & ANM_VM_IGNORE_GAME_SPEED)
    {
        g_game_speed = 1.0f;
    }
    if (get_slowdown_factor() > 0.0f)
    {
        g_game_speed = saved_game_speed - get_slowdown_factor() * saved_game_speed;
        if (g_game_speed < 0.0f)
        {
            g_game_speed = 0.0f;
        }
    }
    if (index_of_on_tick != 0 && g_anm_on_tick_funcs[index_of_on_tick](this) != 0)
    {
        g_game_speed = saved_game_speed;
        return 1;
    }
    if (instr_offset < 0 || (flags_lo & ANM_VM_FLAG_LO_100000))
    {
        g_game_speed = saved_game_speed;
        return 0;
    }
    timer_1c++;
    if (pending_interrupt != 0)
    {
        goto interrupt;
    }
    if ((flags_hi & (ANM_VM_FLAG_HI_4000 | ANM_VM_FLAG_HI_8000)) == ANM_VM_FLAG_HI_4000 && g_GameThread != NULL &&
        g_GameThread->flags.flag_1)
    {
        g_game_speed = saved_game_speed;
        return 0;
    }
    goto run_script;

interrupt:
    {
        // Jump to the label of the pending interrupt, else to label -1.
        AnmRawInstr *fallback = NULL;
        i32 fallback_offset = 0;
        i32 offset = 0;
        ins = (AnmRawInstr *)g_AnmManager->loaded_anms[anm_loaded_index]->scripts[script_id];
        while (!(ins->opcode == 5 && ins->args[0].i == pending_interrupt) && ins->opcode != -1)
        {
            if (ins->opcode == 5 && ins->args[0].i == -1)
            {
                fallback = ins;
                fallback_offset = offset;
            }
            offset += ins->offset_to_next;
            ins = (AnmRawInstr *)((u8 *)ins + ins->offset_to_next);
        }
        flags_lo &= ~ANM_VM_STOPPED;
        pending_interrupt = 0;
        if (ins->opcode != 5)
        {
            if (fallback == NULL)
            {
                goto stop;
            }
            ins = fallback;
            offset = fallback_offset;
        }
        interrupt_return_time.set_from(script_time);
        interrupt_return_offset = instr_offset;
        offset += ins->offset_to_next;
        script_time.set_value(ins->time);
        flags_lo |= ANM_VM_VISIBLE;
        instr_offset = offset;
    }

run_script:
    for (;;)
    {
        ins = (AnmRawInstr *)(g_AnmManager->loaded_anms[anm_loaded_index]->scripts[script_id] + instr_offset);
        if (ins->time > script_time.current)
        {
            goto done;
        }
        switch (ins->opcode)
        {
        // jmp
        case 200:
            script_time.set_value(ins->args[1].i);
            instr_offset = ins->args[0].i;
            continue;
        // jmpDec
        case 201:
            (*ANM_INT_PTR(0))--;
            if (ANM_INT(0) > 0)
            {
                script_time.set_value(ins->args[2].i);
                instr_offset = ins->args[1].i;
                continue;
            }
            break;
        // wait
        case 6:
            script_time.rewind(ANM_INT(0));
            break;
        // caseReturn
        case 7:
            script_time.set_from(interrupt_return_time);
            instr_offset = interrupt_return_offset;
            continue;
        // iset, fset
        case 100:
            *ANM_INT_PTR(0) = ANM_INT(1);
            break;
        case 101:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1);
            break;
        // isetAdd ... fsetMod
        case 112:
            *ANM_INT_PTR(0) = ANM_INT(1) + ANM_INT(2);
            break;
        case 113:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1) + ANM_FLOAT(2);
            break;
        case 114:
            *ANM_INT_PTR(0) = ANM_INT(1) - ANM_INT(2);
            break;
        case 115:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1) - ANM_FLOAT(2);
            break;
        case 116:
            *ANM_INT_PTR(0) = ANM_INT(1) * ANM_INT(2);
            break;
        case 117:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1) * ANM_FLOAT(2);
            break;
        case 118:
            *ANM_INT_PTR(0) = ANM_INT(1) / ANM_INT(2);
            break;
        case 119:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1) / ANM_FLOAT(2);
            break;
        case 120:
            *ANM_INT_PTR(0) = ANM_INT(1) % ANM_INT(2);
            break;
        case 121:
            *ANM_FLOAT_PTR(0) = fmodf(ANM_FLOAT(1), ANM_FLOAT(2));
            break;
        // iadd ... fmod
        case 102:
            *ANM_INT_PTR(0) += ANM_INT(1);
            break;
        case 103:
            *ANM_FLOAT_PTR(0) += ANM_FLOAT(1);
            break;
        case 104:
            *ANM_INT_PTR(0) -= ANM_INT(1);
            break;
        case 105:
            *ANM_FLOAT_PTR(0) -= ANM_FLOAT(1);
            break;
        case 106:
            *ANM_INT_PTR(0) *= ANM_INT(1);
            break;
        case 107:
            *ANM_FLOAT_PTR(0) *= ANM_FLOAT(1);
            break;
        case 108:
            *ANM_INT_PTR(0) /= ANM_INT(1);
            break;
        case 109:
            *ANM_FLOAT_PTR(0) /= ANM_FLOAT(1);
            break;
        case 110:
            *ANM_INT_PTR(0) %= ANM_INT(1);
            break;
        case 111:
            *ANM_FLOAT_PTR(0) = fmodf(ANM_FLOAT(0), ANM_FLOAT(1));
            break;
        // isetRand, fsetRand
        case 122:
        {
            u32 range = ANM_INT(1);
            *ANM_INT_PTR(0) = range != 0 ? g_replay_unsafe_rng.rand_u32() % range : 0;
            break;
        }
        case 123:
            *ANM_FLOAT_PTR(0) = g_replay_unsafe_rng.randf_0_to(ANM_FLOAT(1));
            break;
        // fsin, fcos, ftan, facos, fatan
        case 124:
            *ANM_FLOAT_PTR(0) = sinf(ANM_FLOAT(1));
            break;
        case 125:
            *ANM_FLOAT_PTR(0) = cosf(ANM_FLOAT(1));
            break;
        case 126:
            *ANM_FLOAT_PTR(0) = tanf(ANM_FLOAT(1));
            break;
        case 127:
            *ANM_FLOAT_PTR(0) = acosf(ANM_FLOAT(1));
            break;
        case 128:
            *ANM_FLOAT_PTR(0) = atanf(ANM_FLOAT(1));
            break;
        // validRad
        case 129:
            *ANM_FLOAT_PTR(0) = add_normalize_angle(ANM_FLOAT(0), 0.0f);
            break;
        // circlePos
        case 130:
            anm_sincosmul_xy(ANM_FLOAT_PTR(0), ANM_FLOAT_PTR(1), ANM_FLOAT(2), ANM_FLOAT(3));
            break;
        // circlePosRand
        case 131:
        {
            f32 min = ANM_FLOAT(2);
            f32 max = ANM_FLOAT(3);
            f32 angle = g_replay_unsafe_rng.randf_neg_1_to_1() * ZUN_PI;
            f32 radius = g_replay_unsafe_rng.randf_neg_1_to_1() * (max - min) + min;
            Float3 point;
            anm_sincosmul(&point, angle, radius);
            *ANM_FLOAT_PTR(0) = point.x;
            *ANM_FLOAT_PTR(1) = point.y;
            break;
        }
        // ije ... fjge
        case 202:
            if (ANM_INT(0) == ANM_INT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 203:
            if (ANM_FLOAT(0) == ANM_FLOAT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 204:
            if (ANM_INT(0) != ANM_INT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 205:
            if (ANM_FLOAT(0) != ANM_FLOAT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 206:
            if (ANM_INT(0) < ANM_INT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 207:
            if (ANM_FLOAT(0) < ANM_FLOAT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 208:
            if (ANM_INT(0) <= ANM_INT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 209:
            if (ANM_FLOAT(0) <= ANM_FLOAT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 210:
            if (ANM_INT(0) > ANM_INT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 211:
            if (ANM_FLOAT(0) > ANM_FLOAT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 212:
            if (ANM_INT(0) >= ANM_INT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        case 213:
            if (ANM_FLOAT(0) >= ANM_FLOAT(1))
            {
                script_time.set_value(ins->args[3].i);
                instr_offset = ins->args[2].i;
                continue;
            }
            break;
        // sprite
        case 300:
        {
            flags_lo |= ANM_VM_VISIBLE;
            i32 sprite;
            if (index_of_sprite_mapping_func != 0)
            {
                sprite = g_anm_sprite_mapping_funcs[index_of_sprite_mapping_func](this, ANM_INT(0));
            }
            else
            {
                sprite = ANM_INT(0);
            }
            if (sprite < 0)
            {
                g_AsciiManager->ascii_anm->set_sprite(this, 0x102);
            }
            else
            {
                g_AnmManager->loaded_anms[anm_loaded_index]->set_sprite(this, sprite);
            }
            time_of_last_sprite_set = script_time.current;
            break;
        }
        case 432:
            ANM_FLAGS_HI->ignore_game_speed = ANM_INT(0);
            break;
        // scriptNew
        case 500:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this, 0);
            break;
        // scriptNewPos
        case 505:
        {
            AnmId id = g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this, 0);
            AnmVm *child = id.find_or_clear();
            child->pos_2.x = ANM_FLOAT(1);
            child->pos_2.y = ANM_FLOAT(2);
            break;
        }
        // scriptNewFront, scriptNewUI, scriptNewUIFront
        case 502:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this, 2);
            break;
        case 501:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this, 4);
            break;
        case 503:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this, 6);
            break;
        // copyVars
        case 509:
            if (unk_5b0 != NULL)
            {
                memcpy(int_vars, unk_5b0->int_vars, offsetof(AnmVm, pos_2) - offsetof(AnmVm, int_vars));
            }
            break;
        // scriptNewRootPos
        case 506:
        {
            AnmId id = g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_root(ANM_INT(0), this, 0);
            AnmVm *child = id.find_or_clear();
            child->pos_2.x = ANM_FLOAT(1);
            child->pos_2.y = ANM_FLOAT(2);
            break;
        }
        // scriptNewRoot
        case 504:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_root(ANM_INT(0), this, 0);
            break;
        // effectNew
        case 508:
            g_EffectManager->create_effect(ANM_INT(0), (D3DXVECTOR3 *)this, this);
            break;
        // spriteRand
        case 301:
        {
            flags_lo |= ANM_VM_VISIBLE;
            i32 sprite;
            if (index_of_sprite_mapping_func != 0)
            {
                sprite = g_anm_sprite_mapping_funcs[index_of_sprite_mapping_func](
                    this, ANM_INT(0) + g_replay_unsafe_rng.rand_u32() % ANM_INT(1));
            }
            else
            {
                sprite = ANM_INT(0) + g_replay_unsafe_rng.rand_u32() % ANM_INT(1);
            }
            if (sprite < 0)
            {
                g_AsciiManager->ascii_anm->set_sprite(this, 0x102);
            }
            else
            {
                g_AnmManager->loaded_anms[anm_loaded_index]->set_sprite(this, sprite);
            }
            time_of_last_sprite_set = script_time.current;
            break;
        }
        // scale, scale2, zoomOut
        case 402:
            scale.x = ANM_FLOAT(0);
            scale.y = ANM_FLOAT(1);
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        case 434:
            scale_2.x = ANM_FLOAT(0);
            scale_2.y = ANM_FLOAT(1);
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        case 429:
            uv_scale.x = ANM_FLOAT(0);
            uv_scale.y = ANM_FLOAT(1);
            flags_lo |= ANM_VM_UV_SCALE_CHANGED;
            break;
        // alpha, color, alpha2, color2
        case 403:
            color_1.a = ANM_INT(0);
            break;
        case 404:
            color_1.r = ANM_INT(0);
            color_1.g = ANM_INT(1);
            color_1.b = ANM_INT(2);
            break;
        case 405:
            color_2.a = ANM_INT(0);
            break;
        case 406:
            color_2.r = ANM_INT(0);
            color_2.g = ANM_INT(1);
            color_2.b = ANM_INT(2);
            break;
        // flipX, flipY
        case 308:
            flags_lo ^= ANM_VM_FLAG_LO_800;
            scale.x *= -1.0f;
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        case 309:
            flags_lo ^= ANM_VM_FLAG_LO_1000;
            scale.y *= -1.0f;
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        // colorizeChildren
        case 315:
            ANM_FLAGS_HI->colorize_children = (u8)ins->args[0].i;
            break;
        case 316:
            flags_lo |= ANM_VM_FLAG_LO_2;
            break;
        case 317:
            flags_lo &= ~ANM_VM_FLAG_LO_2;
            break;
        // rotate
        case 401:
            rotation.x = ANM_FLOAT(0);
            rotation.y = ANM_FLOAT(1);
            rotation.z = ANM_FLOAT(2);
            flags_lo |= ANM_VM_ROTATION_CHANGED;
            break;
        // angleVel, scaleGrowth
        case 415:
            angular_velocity = Float3(ANM_FLOAT(0), ANM_FLOAT(1), ANM_FLOAT(2));
            flags_hi |= ANM_VM_HAS_VELOCITY;
            break;
        case 416:
            scale_growth = Float2(ANM_FLOAT(0), ANM_FLOAT(1));
            flags_hi |= ANM_VM_HAS_VELOCITY;
            break;
        // alphaTimeLinear
        case 417:
            set_alpha1_time(ANM_INT(1), INTERP_LINEAR, color_1.a, (u8)ins->args[0].i);
            break;
        // blendMode
        case 303:
            ANM_FLAGS_LO->blend_mode = ins->args[0].i;
            break;
        // pos
        case 400:
            if (!(flags_lo & ANM_VM_POS_I_TO_POS_2))
            {
                pos = Float3(ANM_FLOAT(0), ANM_FLOAT(1), ANM_FLOAT(2));
            }
            else
            {
                pos_2 = Float3(ANM_FLOAT(0), ANM_FLOAT(1), ANM_FLOAT(2));
            }
            break;
        // anchorOffset
        case 436:
            anchor_offset.x = ANM_FLOAT(0);
            anchor_offset.y = ANM_FLOAT(1);
            break;
        // rotationMode
        case 437:
            ANM_FLAGS_HI->rotation_mode = ANM_INT(0);
            break;
        // visible
        case 310:
            ANM_FLAGS_LO->visible = ins->args[0].i;
            break;
        // anchor
        case 421:
            ANM_FLAGS_LO->anchor_x = ((u16 *)ins->args)[0];
            ANM_FLAGS_LO->anchor_y = ((u16 *)ins->args)[1];
            break;
        // scrollX, scrollY
        case 425:
            uv_scroll_vel.x = ANM_FLOAT(0);
            flags_hi |= ANM_VM_HAS_VELOCITY;
            break;
        case 426:
            uv_scroll_vel.y = ANM_FLOAT(0);
            flags_hi |= ANM_VM_HAS_VELOCITY;
            break;
        // zWriteDisable
        case 305:
            ANM_FLAGS_LO->z_write_disable = ins->args[0].i;
            break;
        case 306:
            ANM_FLAGS_LO->follow_camera = ins->args[0].i;
            break;
        // resampleMode
        case 311:
            ANM_FLAGS_HI->filter_point = ins->args[0].i;
            break;
        // posTime
        case 407:
            pos_i.end_time = ANM_INT(0);
            pos_i.bezier_1 = g_zero_vec;
            pos_i.bezier_2 = g_zero_vec;
            pos_i.method = ins->args[1].i;
            pos_i.initial = !(flags_lo & ANM_VM_POS_I_TO_POS_2) ? pos : pos_2;
            pos_i.goal = Float3(ANM_FLOAT(2), ANM_FLOAT(3), ANM_FLOAT(4));
            pos_i.reset_timer();
            break;
        // Like posTime, to a point given by angle and distance.
        case 433:
        {
            pos_i.end_time = ANM_INT(0);
            pos_i.bezier_1 = g_zero_vec;
            pos_i.bezier_2 = g_zero_vec;
            pos_i.method = ins->args[1].i;
            pos_i.initial = !(flags_lo & ANM_VM_POS_I_TO_POS_2) ? pos : pos_2;
            Float3 goal;
            anm_sincosmul(&goal, ANM_FLOAT(2), ANM_FLOAT(3));
            goal.z = 0.0f;
            pos_i.goal = goal;
            pos_i.reset_timer();
            break;
        }
        // moveBezier
        case 420:
        {
            Float3 bezier_1;
            Float3 bezier_2;
            bezier_1.x = ANM_FLOAT(1);
            bezier_1.y = ANM_FLOAT(2);
            bezier_1.z = ANM_FLOAT(3);
            bezier_2.x = ANM_FLOAT(7);
            bezier_2.y = ANM_FLOAT(8);
            bezier_2.z = ANM_FLOAT(9);
            pos_i.end_time = ANM_INT(0);
            pos_i.bezier_1 = bezier_1;
            pos_i.bezier_2 = bezier_2;
            pos_i.method = INTERP_BEZIER;
            pos_i.initial = !(flags_lo & ANM_VM_POS_I_TO_POS_2) ? pos : pos_2;
            pos_i.goal = Float3(ANM_FLOAT(4), ANM_FLOAT(5), ANM_FLOAT(6));
            pos_i.reset_timer();
            break;
        }
        // colorTime, alphaTime, color2Time, alpha2Time
        case 408:
        {
            ZunColor initial = anm_rgb(color_1.r, color_1.g, color_1.b);
            ZunColor goal = anm_rgb(ANM_INT(2), ANM_INT(3), ANM_INT(4));
            set_rgb1_time(ANM_INT(0), (u8)ins->args[1].i, &initial, &goal);
            break;
        }
        case 409:
            set_alpha1_time(ANM_INT(0), (u8)ins->args[1].i, color_1.a, ANM_INT(2));
            break;
        case 413:
        {
            ZunColor initial = anm_rgb(color_2.r, color_2.g, color_2.b);
            ZunColor goal = anm_rgb(ANM_INT(2), ANM_INT(3), ANM_INT(4));
            set_rgb2_time(ANM_INT(0), (u8)ins->args[1].i, &initial, &goal);
            break;
        }
        case 414:
            set_alpha2_time(ANM_INT(0), (u8)ins->args[1].i, color_2.a, ANM_INT(2));
            break;
        // rotateTime
        case 410:
        {
            Float3 goal(ANM_FLOAT(2), ANM_FLOAT(3), ANM_FLOAT(4));
            rotate_i.end_time = ANM_INT(0);
            rotate_i.bezier_1 = g_zero_vec;
            rotate_i.bezier_2 = g_zero_vec;
            rotate_i.method = ins->args[1].i;
            rotate_i.initial = rotation;
            rotate_i.goal = goal;
            rotate_i.reset_timer();
            flags_lo |= ANM_VM_ROTATION_CHANGED;
            break;
        }
        // rotateTime2D
        case 411:
        {
            ZunAngle goal(ANM_FLOAT(2));
            ZunAngle initial(rotation.z);
            ZunAngle zero(0.0f);
            rotate_2d_i.end_time = ANM_INT(0);
            rotate_2d_i.bezier_1 = zero;
            rotate_2d_i.bezier_2 = zero;
            rotate_2d_i.method = ins->args[1].i;
            rotate_2d_i.initial = initial;
            rotate_2d_i.goal = goal;
            rotate_2d_i.reset_time();
            flags_lo |= ANM_VM_ROTATION_CHANGED;
            break;
        }
        // scaleTime, scale2Time (which starts from scale, not scale_2),
        // zoomOutTime
        case 412:
        {
            Float2 goal(ANM_FLOAT(2), ANM_FLOAT(3));
            set_scale_interp(ANM_INT(0), (u8)ins->args[1].i, &scale, &goal);
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        }
        case 435:
        {
            Float2 goal(ANM_FLOAT(2), ANM_FLOAT(3));
            set_434_time(ANM_INT(0), (u8)ins->args[1].i, &scale, &goal);
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        }
        case 430:
        {
            Float2 goal(ANM_FLOAT(2), ANM_FLOAT(3));
            set_uv_scale_time(ANM_INT(0), (u8)ins->args[1].i, &uv_scale, &goal);
            flags_lo |= ANM_VM_UV_SCALE_CHANGED;
            break;
        }
        // scrollXTime, scrollYTime
        case 427:
        {
            f32 goal = ANM_FLOAT(2);
            u_vel_i.end_time = ANM_INT(0);
            u_vel_i.bezier_1 = 0.0f;
            u_vel_i.bezier_2 = 0.0f;
            u_vel_i.method = ins->args[1].i;
            u_vel_i.initial = uv_scroll_vel.x;
            u_vel_i.goal = goal;
            u_vel_i.reset();
            break;
        }
        case 428:
        {
            f32 goal = ANM_FLOAT(2);
            v_vel_i.end_time = ANM_INT(0);
            v_vel_i.bezier_1 = 0.0f;
            v_vel_i.bezier_2 = 0.0f;
            v_vel_i.method = ins->args[1].i;
            v_vel_i.initial = uv_scroll_vel.y;
            v_vel_i.goal = goal;
            v_vel_i.reset();
            break;
        }
        // type
        case 302:
            ANM_FLAGS_LO->render_mode = ins->args[0].i;
            if (ANM_FLAGS_LO->render_mode == 10)
            {
                anm_effect_4_init(this);
            }
            break;
        // Moves entity_pos into pos.
        case 422:
            pos = entity_pos;
            entity_pos.x = 0.0f;
            entity_pos.y = 0.0f;
            entity_pos.z = 0.0f;
            break;
        // texCircle, texArcEven, texArc
        case 600:
            ANM_FLAGS_LO->render_mode = 9;
            alloc_extra_data(ANM_INT(0) * 56);
            break;
        case 601:
            ANM_FLAGS_LO->render_mode = 13;
            alloc_extra_data(ANM_INT(0) * 56);
            break;
        case 602:
            ANM_FLAGS_LO->render_mode = 14;
            alloc_extra_data(ANM_INT(0) * 56);
            break;
        // texCylinder3D, texRing3D
        case 609:
            ANM_FLAGS_LO->render_mode = 24;
            alloc_extra_data(ANM_INT(0) * 48);
            break;
        case 610:
            ANM_FLAGS_LO->render_mode = 25;
            alloc_extra_data(ANM_INT(0) * 48);
            break;
        // UVs from the sprite's current corners.
        case 418:
        {
            Float3 corners[4];
            write_sprite_corners(corners);
            divide_vec2_by_640_480(&uv_quad_of_sprite[0], (Float2 *)&corners[0]);
            divide_vec2_by_640_480(&uv_quad_of_sprite[1], (Float2 *)&corners[1]);
            divide_vec2_by_640_480(&uv_quad_of_sprite[2], (Float2 *)&corners[2]);
            divide_vec2_by_640_480(&uv_quad_of_sprite[3], (Float2 *)&corners[3]);
            break;
        }
        case 419:
            ANM_FLAGS_HI->uv_quad_from_corners = ANM_INT(0);
            break;
        // drawRect, drawRectGrad, drawRectRot, drawRectRotGrad, drawLine,
        // drawRectBorder
        case 603:
            ANM_FLAGS_LO->render_mode = 16;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case 606:
            ANM_FLAGS_LO->render_mode = 20;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case 607:
            ANM_FLAGS_LO->render_mode = 21;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case 608:
            ANM_FLAGS_LO->render_mode = 22;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case 613:
            ANM_FLAGS_LO->render_mode = 26;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case 612:
            ANM_FLAGS_LO->render_mode = 27;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        // drawPoly, drawPolyBorder, drawRing
        case 604:
            ANM_FLAGS_LO->render_mode = 17;
            sprite_size.x = ANM_FLOAT(0);
            int_vars[0] = ANM_INT(1);
            break;
        case 605:
            ANM_FLAGS_LO->render_mode = 18;
            sprite_size.x = ANM_FLOAT(0);
            int_vars[0] = ANM_INT(1);
            break;
        case 611:
            ANM_FLAGS_LO->render_mode = 19;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            int_vars[0] = ANM_INT(2);
            break;
        case 507:
            ANM_FLAGS_HI->no_parent_pos = ANM_INT(0);
            break;
        // scrollMode
        case 312:
            ANM_FLAGS_HI->address_u = ANM_INT(0);
            ANM_FLAGS_LO->address_v = ANM_INT(1);
            break;
        // resolutionMode
        case 313:
            ANM_FLAGS_HI->resolution_mode = ANM_INT(0);
            break;
        case 314:
            ANM_FLAGS_HI->rotate_with_parent = ANM_INT(0);
            break;
        // originMode
        case 438:
            ANM_FLAGS_HI->origin_mode = (u8)ins->args[0].i;
            break;
        case 431:
            ANM_FLAGS_HI->flag_8 = (u8)ins->args[0].i;
            break;
        // layer
        case 304:
            set_layer((u8)ins->args[0].i);
            break;
        // colorMode
        case 423:
            ANM_FLAGS_LO->color_mode = (u8)ins->args[0].i;
            break;
        // rotateAuto
        case 424:
            ANM_FLAGS_HI->auto_rotate = (u8)ins->args[0].i;
            break;
        // randMode
        case 307:
            ANM_FLAGS_HI->rand_mode = (u8)ins->args[0].i;
            break;
        // stopHide, stop
        case 4:
            flags_lo &= ~ANM_VM_VISIBLE;
        case 3:
            if (pending_interrupt != 0)
            {
                goto interrupt;
            }
            flags_lo |= ANM_VM_STOPPED;
            goto stop;
        // delete
        case -1:
        case 1:
            flags_lo &= ~ANM_VM_VISIBLE;
            instr_offset = -1;
            g_game_speed = saved_game_speed;
            return 1;
        // static
        case 2:
            instr_offset = -1;
            g_game_speed = saved_game_speed;
            return 0;
        }
        instr_offset += ins->offset_to_next;
    }

stop:
    script_time--;
done:
    if (flags_hi & ANM_VM_HAS_VELOCITY)
    {
        step_velocities();
    }
    if (flags_lo & ANM_VM_FOLLOW_CAMERA)
    {
        entity_pos += g_Supervisor.cameras[3].unk_104;
    }
    if (flags_hi & ANM_VM_UV_QUAD_FROM_CORNERS)
    {
        Float3 corners[4];
        write_sprite_corners(corners);
        divide_vec2_by_640_480(&uv_quad_of_sprite[0], (Float2 *)&corners[0]);
        divide_vec2_by_640_480(&uv_quad_of_sprite[1], (Float2 *)&corners[1]);
        divide_vec2_by_640_480(&uv_quad_of_sprite[2], (Float2 *)&corners[2]);
        divide_vec2_by_640_480(&uv_quad_of_sprite[3], (Float2 *)&corners[3]);
    }
    step_interpolators();
    update_special_vertices();
    if (g_anm_on_wait_funcs[index_of_on_wait] != NULL && g_anm_on_wait_funcs[index_of_on_wait](this) != 0)
    {
        g_game_speed = saved_game_speed;
        return 1;
    }
    script_time.tick();
    g_game_speed = saved_game_speed;
    return 0;
}

// FUNCTION: TH16 0x464dd0
Float3 *AnmVm::get_total_rotation()
{
    rotation_related = rotation;
    if (parent != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
    {
        rotation_related += *parent->get_total_rotation();
        rotation.x = wrap_angle(rotation.x);
        rotation.y = wrap_angle(rotation.y);
        rotation.z = wrap_angle(rotation.z);
    }
    return &rotation_related;
}

// FUNCTION: TH16 0x464960
void AnmVm::set_uv_scale_time(i32 end_time, i32 method, Float2 *initial, Float2 *goal)
{
    uv_scale_i.end_time = end_time;
    uv_scale_i.method = method;
    uv_scale_i.initial = *initial;
    uv_scale_i.goal = *goal;
    uv_scale_i.time = 0;
}

// FUNCTION: TH16 0x464a00
void AnmVm::set_434_time(i32 end_time, i32 method, Float2 *initial, Float2 *goal)
{
    op_434_i.end_time = end_time;
    op_434_i.method = method;
    op_434_i.initial = *initial;
    op_434_i.goal = *goal;
    op_434_i.time = 0;
}

// FUNCTION: TH16 0x464aa0
void AnmVm::set_alpha2_time(i32 end_time, i32 method, u8 initial, u8 goal)
{
    alpha2_i.end_time = end_time;
    alpha2_i.method = method;
    alpha2_i.initial = initial;
    alpha2_i.goal = goal;
    alpha2_i.time = 0;
    flags_lo = flags_lo & ~ANM_VM_COLOR_MODE_MASK | ANM_VM_COLOR_MODE_1;
}

// TODO: the original loads both colors before storing either (y, z, x order) and keeps this in edi.
// FUNCTION: TH16 0x464b40
void AnmVm::set_rgb2_time(i32 end_time, i32 method, ZunColor *initial, ZunColor *goal)
{
    rgb2_i.end_time = end_time;
    rgb2_i.bezier_2 = rgb2_i.bezier_1 = Int3(0, 0, 0);
    rgb2_i.method = method;
    Int3 a(initial->b, initial->g, initial->r);
    Int3 b(goal->b, goal->g, goal->r);
    rgb2_i.initial = a;
    rgb2_i.goal = b;
    rgb2_i.time = 0;
    flags_lo = flags_lo & ~ANM_VM_COLOR_MODE_MASK | ANM_VM_COLOR_MODE_1;
}

// TODO: same color load order difference as set_rgb2_time.
// FUNCTION: TH16 0x464c60
void AnmVm::set_rgb1_time(i32 end_time, i32 method, ZunColor *initial, ZunColor *goal)
{
    rgb1_i.end_time = end_time;
    rgb1_i.bezier_2 = rgb1_i.bezier_1 = Int3(0, 0, 0);
    rgb1_i.method = method;
    Int3 a(initial->b, initial->g, initial->r);
    Int3 b(goal->b, goal->g, goal->r);
    rgb1_i.initial = a;
    rgb1_i.goal = b;
    rgb1_i.time = 0;
}

// TODO: /GS cookie from the stubbed InterpFloat3/InterpAngle steps, and the 8-byte frame alignment InterpFloat2::step has too.
// FUNCTION: TH16 0x463b30
void AnmVm::step_interpolators()
{
    if (pos_i.end_time != 0)
    {
        if (!(flags_lo & ANM_VM_POS_I_TO_POS_2))
        {
            pos = pos_i.step();
        }
        else
        {
            pos_2 = pos_i.step();
        }
    }
    if (rgb1_i.end_time != 0)
    {
        Int3 c = rgb1_i.step();
        color_1.r = c.z;
        color_1.g = c.y;
        color_1.b = c.x;
    }
    if (alpha1_i.end_time != 0)
    {
        color_1.a = alpha1_i.step();
    }
    if (scale_i.end_time != 0)
    {
        scale = scale_i.step();
        flags_lo |= ANM_VM_SCALE_CHANGED;
    }
    if (op_434_i.end_time != 0)
    {
        scale_2 = op_434_i.step();
        flags_lo |= ANM_VM_SCALE_CHANGED;
    }
    if (uv_scale_i.end_time != 0)
    {
        uv_scale = uv_scale_i.step();
        flags_lo |= ANM_VM_UV_SCALE_CHANGED;
    }
    if (rotate_i.end_time != 0)
    {
        rotation = rotate_i.step();
        flags_lo |= ANM_VM_ROTATION_CHANGED;
    }
    if (rotate_2d_i.end_time != 0)
    {
        rotation.z = rotate_2d_i.step().value;
        flags_lo |= ANM_VM_ROTATION_CHANGED;
    }
    if (rgb2_i.end_time != 0)
    {
        Int3 c = rgb2_i.step();
        color_2.r = c.z;
        color_2.g = c.y;
        color_2.b = c.x;
    }
    if (alpha2_i.end_time != 0)
    {
        color_2.a = alpha2_i.step();
    }
    if (u_vel_i.end_time != 0)
    {
        uv_scroll_vel.x = u_vel_i.step();
    }
    if (v_vel_i.end_time != 0)
    {
        uv_scroll_vel.y = v_vel_i.step();
    }
}

// FUNCTION: TH16 0x4630f0
void AnmVm::step_velocities()
{
    if (angular_velocity.x != 0.0f)
    {
        rotation.x = wrap_angle(rotation.x + angular_velocity.x * g_game_speed);
        flags_lo |= ANM_VM_ROTATION_CHANGED;
    }
    if (angular_velocity.y != 0.0f)
    {
        rotation.y = wrap_angle(rotation.y + angular_velocity.y * g_game_speed);
        flags_lo |= ANM_VM_ROTATION_CHANGED;
    }
    if (angular_velocity.z != 0.0f)
    {
        rotation.z = wrap_angle(rotation.z + angular_velocity.z * g_game_speed);
        flags_lo |= ANM_VM_ROTATION_CHANGED;
    }
    if (scale_growth.y != 0.0f)
    {
        scale.y += scale_growth.y * g_game_speed;
        flags_lo |= ANM_VM_SCALE_CHANGED;
    }
    if (scale_growth.x != 0.0f)
    {
        scale.x += scale_growth.x * g_game_speed;
        flags_lo |= ANM_VM_SCALE_CHANGED;
    }
    if (uv_scroll_vel.x != 0.0f)
    {
        uv_scroll_pos.x += uv_scroll_vel.x * g_game_speed;
        if (uv_scroll_pos.x >= 2.0f)
        {
            uv_scroll_pos.x -= 2.0f;
        }
        else if (uv_scroll_pos.x < 0.0f)
        {
            uv_scroll_pos.x += 2.0f;
        }
    }
    if (uv_scroll_vel.y != 0.0f)
    {
        uv_scroll_pos.y += uv_scroll_vel.y * g_game_speed;
        if (uv_scroll_pos.y >= 2.0f)
        {
            uv_scroll_pos.y -= 2.0f;
        }
        else if (uv_scroll_pos.y < 0.0f)
        {
            uv_scroll_pos.y += 2.0f;
        }
    }
}
