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

static_assert(offsetof(AnmVm, total_rotation) == 0x5f0, "AnmVm layout");
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
        return float_vars_4_to_6.x;
    case ANM_VAR_F5:
        return float_vars_4_to_6.y;
    case ANM_VAR_F6:
        return float_vars_4_to_6.z;
    case ANM_VAR_I4:
        return int_var_4;
    case ANM_VAR_I5:
        return int_var_5;
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
        return float_vars_4_to_6.x;
    case ANM_VAR_F5:
        return float_vars_4_to_6.y;
    case ANM_VAR_F6:
        return float_vars_4_to_6.z;
    case ANM_VAR_I4:
        return int_var_4;
    case ANM_VAR_I5:
        return int_var_5;
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
        return &float_vars_4_to_6.x;
    case ANM_VAR_F5:
        return &float_vars_4_to_6.y;
    case ANM_VAR_F6:
        return &float_vars_4_to_6.z;
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
        return &int_var_4;
    case ANM_VAR_I5:
        return &int_var_5;
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

// Rebuilds the vertices that render modes 9, 13, 14, 24 and 25 draw from
// extra_data: a ring strip around the VM (9), an arc of it (13,
// 14) and an upright cylinder band (24, 25), int_vars[0] steps around
// with the texture's u spread over int_vars[1].
// TODO: register allocation differs (the original keeps this in edi and the vertex cursor on the stack in mode 9).
// FUNCTION: TH16 0x4632f0
void AnmVm::update_special_vertices()
{
    switch ((flags_lo >> ANM_VM_RENDER_MODE_SHIFT) & 0x1f)
    {
    case ANM_RENDER_TEX_CIRCLE: {
        i32 n = int_vars[0] - 1;
        f32 angle = rotation.z;
        RenderVertex144 *vertex = (RenderVertex144 *)extra_data;
        f32 angle_step = ZUN_2PI / n;
        f32 v_step = (f32)int_vars[1] / n;
        Float3 pos;
        get_own_transformed_pos(&pos);
        D3DCOLOR color_outer = color_1.d3d;
        D3DCOLOR color_inner = (flags_lo & ANM_VM_COLOR_MODE_MASK) ? color_2.d3d : color_1.d3d;
        f32 half_width = scale.x * 0.5f;
        f32 outer = scale.y + half_width;
        f32 inner = scale.y - half_width;
        if (parent_vm != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
        {
            outer *= parent_vm->scale.x;
            inner *= parent_vm->scale.y;
        }
        if ((flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_SCALED)
        {
            outer *= g_screen_coord_scale;
            inner *= g_screen_coord_scale;
        }
        else if ((flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_HALF_SCALED)
        {
            outer *= g_screen_coord_scale * 0.5f;
            inner *= g_screen_coord_scale * 0.5f;
        }
        f32 v = 0.0f;
        for (; n > 0; n--)
        {
            vertex->pos.w = 1.0f;
            vertex->diffuse = color_outer;
            vertex->uv.x = uv_scroll_pos.x + uv_quad_of_sprite[0].x;
            vertex->uv.y = uv_scroll_pos.y + v;
            anm_sincosmul((Float3 *)&vertex->pos, angle, outer);
            vertex->pos.z = 0.0f;
            vertex->pos.x = vertex->pos.x + pos.x;
            vertex->pos.y = pos.y + vertex->pos.y;
            vertex->pos.z = vertex->pos.z + pos.z;
            vertex++;
            vertex->pos.w = 1.0f;
            vertex->diffuse = color_inner;
            vertex->uv.x = uv_quad_of_sprite[1].x + uv_scroll_pos.x;
            vertex->uv.y = uv_scroll_pos.y + v;
            anm_sincosmul((Float3 *)&vertex->pos, angle, inner);
            vertex->pos.z = 0.0f;
            vertex->pos.x = vertex->pos.x + pos.x;
            vertex->pos.y = pos.y + vertex->pos.y;
            vertex->pos.z = vertex->pos.z + pos.z;
            v += v_step;
            angle += angle_step;
            vertex++;
            angle = wrap_angle(angle);
        }
        RenderVertex144 *first = (RenderVertex144 *)extra_data;
        vertex[0] = first[0];
        vertex[0].uv.y = uv_scroll_pos.y + v;
        first = (RenderVertex144 *)extra_data;
        vertex[1] = first[1];
        vertex[1].uv.y = uv_scroll_pos.y + v;
        break;
    }
    case ANM_RENDER_TEX_ARC_EVEN:
    case ANM_RENDER_TEX_ARC: {
        f32 start = wrap_angle(rotation.z - rotation.x * 0.5f);
        i32 n = int_vars[0];
        f32 v = 0.0f;
        RenderVertex144 *vertex = (RenderVertex144 *)extra_data;
        f32 angle_step = rotation.x / (n - 1);
        f32 v_step = (f32)int_vars[1] / (n - 1);
        Float3 pos;
        get_own_transformed_pos(&pos);
        f32 angle;
        if ((flags_lo & (0x1f << ANM_VM_RENDER_MODE_SHIFT)) == ANM_RENDER_TEX_ARC << ANM_VM_RENDER_MODE_SHIFT)
        {
            angle = normalize_angle(rotation.z);
        }
        else
        {
            angle = start;
        }
        D3DCOLOR color = (flags_lo & ANM_VM_COLOR_MODE_MASK) ? color_2.d3d : color_1.d3d;
        f32 half_width = scale.x * 0.5f;
        f32 outer = scale.y + half_width;
        f32 inner = scale.y - half_width;
        if (parent_vm != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
        {
            outer *= parent_vm->scale.x;
            inner *= parent_vm->scale.y;
        }
        if ((flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_SCALED)
        {
            outer *= g_screen_coord_scale;
            inner *= g_screen_coord_scale;
        }
        else if ((flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_HALF_SCALED)
        {
            outer *= g_screen_coord_scale * 0.5f;
            inner *= g_screen_coord_scale * 0.5f;
        }
        for (; n > 0; n--)
        {
            vertex[0].pos.w = 1.0f;
            vertex[0].diffuse = color;
            vertex[0].uv.x = uv_scroll_pos.x + uv_quad_of_sprite[0].x;
            vertex[0].uv.y = uv_scroll_pos.y + v;
            anm_sincosmul((Float3 *)&vertex[0].pos, angle, outer);
            vertex[0].pos.z = 0.0f;
            vertex[0].pos.x = pos.x + vertex[0].pos.x;
            vertex[0].pos.y = vertex[0].pos.y + pos.y;
            vertex[0].pos.z = pos.z + vertex[0].pos.z;
            vertex[1].pos.w = 1.0f;
            vertex[1].diffuse = color;
            vertex[1].uv.x = uv_quad_of_sprite[1].x + uv_scroll_pos.x;
            vertex[1].uv.y = uv_scroll_pos.y + v;
            anm_sincosmul((Float3 *)&vertex[1].pos, angle, inner);
            vertex[1].pos.z = 0.0f;
            vertex[1].pos.x = pos.x + vertex[1].pos.x;
            vertex[1].pos.y = vertex[1].pos.y + pos.y;
            vertex[1].pos.z = pos.z + vertex[1].pos.z;
            vertex += 2;
            v = v_step + v;
            angle += angle_step;
            angle = wrap_angle(angle);
        }
        break;
    }
    case ANM_RENDER_TEX_CYLINDER_3D:
    case ANM_RENDER_TEX_RING_3D: {
        f32 width = float_vars[0];
        f32 angle = wrap_angle(float_vars[3] - width * 0.5f);
        i32 n = int_vars[0];
        f32 angle_step = width / (n - 1);
        f32 v = 0.0f;
        RenderVertexXyzDiffuseTex *vertex = (RenderVertexXyzDiffuseTex *)extra_data;
        f32 v_step = (f32)int_vars[1] / (n - 1);
        D3DCOLOR color = (flags_lo & ANM_VM_COLOR_MODE_MASK) ? color_2.d3d : color_1.d3d;
        f32 y = float_vars[1] * 0.5f;
        f32 radius_top = float_vars[2];
        f32 radius_bottom = float_vars[2];
        if ((flags_lo & (0x1f << ANM_VM_RENDER_MODE_SHIFT)) == ANM_RENDER_TEX_RING_3D << ANM_VM_RENDER_MODE_SHIFT)
        {
            radius_top = radius_bottom - y;
            radius_bottom = y + radius_bottom;
            y = 0.0f;
        }
        for (; n > 0; n--)
        {
            Float3 point;
            anm_sincosmul(&point, angle, radius_top);
            vertex[0].diffuse = color;
            vertex[0].uv.x = uv_scroll_pos.x + uv_quad_of_sprite[0].x;
            vertex[0].uv.y = uv_scroll_pos.y + v;
            vertex[0].pos.x = point.x;
            vertex[0].pos.y = y;
            vertex[0].pos.z = point.y;
            anm_sincosmul(&point, angle, radius_bottom);
            vertex[1].diffuse = color;
            vertex[1].uv.x = uv_quad_of_sprite[1].x + uv_scroll_pos.x;
            vertex[1].uv.y = uv_scroll_pos.y + v;
            v += v_step;
            vertex[1].pos.x = point.x;
            vertex[1].pos.y = -y;
            vertex[1].pos.z = point.y;
            angle += angle_step;
            vertex += 2;
            angle = wrap_angle(angle);
        }
        break;
    }
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

// Bytes of extra data per segment of the special render modes: an outer and
// an inner vertex, in screen space (texCircle, texArc) or in 3D
// (texCylinder3D, texRing3D).
enum
{
    ANM_SCREEN_SEGMENT_SIZE = 2 * sizeof(RenderVertex144),
    ANM_3D_SEGMENT_SIZE = 2 * sizeof(RenderVertexXyzDiffuseTex),
};

// Stores to float argument n (a variable or the argument itself), with the
// value computed first.
static __forceinline void anm_store_float(AnmVm *vm, AnmRawInstr *ins, i32 n, f32 value)
{
    *((ins->var_mask & (1 << n)) ? vm->get_float_var_ptr(&ins->args[n].f) : &ins->args[n].f) = value;
}

// From a to b as t goes from 0 to 1.
static inline f32 anm_lerp(f32 t, f32 a, f32 b)
{
    return t * (b - a) + a;
}


// The color arguments of instructions 408 and 413 (and the current color),
// as set_rgb1_time and set_rgb2_time take them. Alpha is left unset.
struct AnmRgb
{
    u8 b;
    u8 g;
    u8 r;
};

static inline void anm_rgb(AnmRgb *c, i32 r, i32 g, i32 b)
{
    c->b = b;
    c->g = g;
    c->r = r;
}

// 0x469e20. Sets up render mode 10 (ANM instruction 302): extra data and
// the on_tick and on_draw callbacks 4 and 6.
int __fastcall anm_fan_init(AnmVm *vm);

// The interpreter proper: runs the instructions due by the current time,
// then steps everything that changes on its own. 1 once the VM should be
// deleted.
__forceinline i32 AnmVm::run_script()
{
    AnmRawInstr *ins;
    i32 result = 0;
    if (instr_offset < 0 || (flags_lo & ANM_VM_SCRIPT_DISABLED))
    {
        return 0;
    }
    time_in_script++;
    if (pending_interrupt == 0)
    {
        if ((flags_hi & (ANM_VM_FREEZES_WITH_WORLD | ANM_VM_FREEZES_AFTER_FIRST_RUN)) == ANM_VM_FREEZES_WITH_WORLD &&
            g_GameThread != NULL && g_GameThread->flags.flag_1)
        {
            return 0;
        }
    }
    else
    {
    interrupt:
        // Jump to the label of the pending interrupt, else to label -1.
        i32 fallback_offset = 0;
        i32 offset = 0;
        AnmRawInstr *fallback = NULL;
        ins = (AnmRawInstr *)g_AnmManager->loaded_anms[anm_loaded_index]->scripts[script_id];
        while (!(ins->opcode == ANM_OP_INTERRUPT_LABEL && pending_interrupt == ins->args[0].i) &&
               ins->opcode != ANM_OP_END)
        {
            if (ins->opcode == ANM_OP_INTERRUPT_LABEL && ins->args[0].i == -1)
            {
                fallback = ins;
                fallback_offset = offset;
            }
            offset += ins->offset_to_next;
            ins = (AnmRawInstr *)((u8 *)ins + ins->offset_to_next);
        }
        flags_lo &= ~ANM_VM_STOPPED;
        pending_interrupt = 0;
        if (ins->opcode != ANM_OP_INTERRUPT_LABEL)
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

    for (;;)
    {
        ins = (AnmRawInstr *)(g_AnmManager->loaded_anms[anm_loaded_index]->scripts[script_id] + instr_offset);
        if (ins->time > script_time.current)
        {
            goto done;
        }
        switch (ins->opcode)
        {
        case ANM_OP_JMP:
            script_time.set_value(ins->args[1].i);
            instr_offset = ins->args[0].i;
            continue;
        case ANM_OP_JMP_DEC:
            (*(!ANM_IS_VAR(0) ? &ins->args[0].i : get_int_var_ptr(&ins->args[0].i)))--;
            if (ANM_INT(0) > 0)
            {
                script_time.set_value(ins->args[2].i);
                instr_offset = ins->args[1].i;
                continue;
            }
            break;
        case ANM_OP_WAIT:
            script_time.rewind(ANM_INT(0));
            break;
        case ANM_OP_CASE_RETURN:
            script_time.set_from(interrupt_return_time);
            instr_offset = interrupt_return_offset;
            continue;
        case ANM_OP_ISET:
            *ANM_INT_PTR(0) = ANM_INT(1);
            break;
        case ANM_OP_FSET:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1);
            break;
        case ANM_OP_ISET_ADD:
        {
            i32 a = ANM_INT(1);
            i32 b = ANM_INT(2);
            i32 *p = &ins->args[0].i;
            if (ANM_IS_VAR(0))
            {
                p = get_int_var_ptr(p);
            }
            *p = a + b;
            break;
        }
        case ANM_OP_FSET_ADD:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1) + ANM_FLOAT(2);
            break;
        case ANM_OP_ISET_SUB:
            *ANM_INT_PTR(0) = ANM_INT(1) - ANM_INT(2);
            break;
        case ANM_OP_FSET_SUB:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1) - ANM_FLOAT(2);
            break;
        case ANM_OP_ISET_MUL:
            *ANM_INT_PTR(0) = ANM_INT(1) * ANM_INT(2);
            break;
        case ANM_OP_FSET_MUL:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1) * ANM_FLOAT(2);
            break;
        case ANM_OP_ISET_DIV:
        {
            i32 a = ANM_INT(1);
            i32 b = ANM_INT(2);
            i32 *p = &ins->args[0].i;
            if (ANM_IS_VAR(0))
            {
                p = get_int_var_ptr(p);
            }
            *p = a / b;
            break;
        }
        case ANM_OP_FSET_DIV:
            *ANM_FLOAT_PTR(0) = ANM_FLOAT(1) / ANM_FLOAT(2);
            break;
        case ANM_OP_ISET_MOD:
        {
            i32 a = ANM_INT(1);
            i32 b = ANM_INT(2);
            i32 *p = &ins->args[0].i;
            if (ANM_IS_VAR(0))
            {
                p = get_int_var_ptr(p);
            }
            *p = a % b;
            break;
        }
        case ANM_OP_FSET_MOD:
            anm_store_float(this, ins, 0, fmodf(ANM_FLOAT(1), ANM_FLOAT(2)));
            break;
        case ANM_OP_IADD:
        {
            i32 value = ANM_INT(1);
            *ANM_INT_PTR(0) += value;
            break;
        }
        case ANM_OP_FADD:
        {
            f32 value = ANM_FLOAT(1);
            *ANM_FLOAT_PTR(0) += value;
            break;
        }
        case ANM_OP_ISUB:
        {
            i32 value = ANM_INT(1);
            *ANM_INT_PTR(0) -= value;
            break;
        }
        case ANM_OP_FSUB:
        {
            f32 value = ANM_FLOAT(1);
            *ANM_FLOAT_PTR(0) -= value;
            break;
        }
        case ANM_OP_IMUL:
        {
            i32 value = ANM_INT(1);
            i32 *p = &ins->args[0].i;
            if (ANM_IS_VAR(0))
            {
                p = get_int_var_ptr(p);
            }
            *p *= value;
            break;
        }
        case ANM_OP_FMUL:
        {
            f32 value = ANM_FLOAT(1);
            *ANM_FLOAT_PTR(0) *= value;
            break;
        }
        case ANM_OP_IDIV:
        {
            i32 value = ANM_INT(1);
            i32 *p = &ins->args[0].i;
            if (ANM_IS_VAR(0))
            {
                p = get_int_var_ptr(p);
            }
            *p /= value;
            break;
        }
        case ANM_OP_FDIV:
        {
            f32 value = ANM_FLOAT(1);
            *ANM_FLOAT_PTR(0) /= value;
            break;
        }
        case ANM_OP_IMOD:
        {
            i32 value = ANM_INT(1);
            i32 *p = &ins->args[0].i;
            if (ANM_IS_VAR(0))
            {
                p = get_int_var_ptr(p);
            }
            *p %= value;
            break;
        }
        case ANM_OP_FMOD:
            anm_store_float(this, ins, 0, fmodf(ANM_FLOAT(0), ANM_FLOAT(1)));
            break;
        case ANM_OP_ISET_RAND:
        {
            u32 range = ANM_INT(1);
            *ANM_INT_PTR(0) = range != 0 ? g_replay_unsafe_rng.rand_u32() % range : 0;
            break;
        }
        case ANM_OP_FSET_RAND:
            anm_store_float(this, ins, 0, g_replay_unsafe_rng.randf_0_to(ANM_FLOAT(1)));
            break;
        case ANM_OP_FSIN:
        {
            f32 value = sinf(ANM_FLOAT(1));
            *ANM_FLOAT_PTR(0) = value;
            break;
        }
        case ANM_OP_FCOS:
        {
            f32 value = cosf(ANM_FLOAT(1));
            *ANM_FLOAT_PTR(0) = value;
            break;
        }
        case ANM_OP_FTAN:
        {
            f32 value = tanf(ANM_FLOAT(1));
            *ANM_FLOAT_PTR(0) = value;
            break;
        }
        case ANM_OP_FACOS:
        {
            f32 value = acosf(ANM_FLOAT(1));
            *ANM_FLOAT_PTR(0) = value;
            break;
        }
        case ANM_OP_FATAN:
        {
            f32 value = atanf(ANM_FLOAT(1));
            *ANM_FLOAT_PTR(0) = value;
            break;
        }
        case ANM_OP_VALID_RAD:
            *ANM_FLOAT_PTR(0) = add_normalize_angle(ANM_FLOAT(0), 0.0f);
            break;
        case ANM_OP_CIRCLE_POS:
            anm_sincosmul_xy(ANM_FLOAT_PTR(0), ANM_FLOAT_PTR(1), ANM_FLOAT(2), ANM_FLOAT(3));
            break;
        case ANM_OP_CIRCLE_POS_RAND:
        {
            f32 min = ANM_FLOAT(2);
            f32 max = ANM_FLOAT(3);
            f32 angle = g_replay_unsafe_rng.randf_neg_1_to_1() * ZUN_PI;
            f32 radius = anm_lerp(g_replay_unsafe_rng.randf_neg_1_to_1(), min, max);
            Float3 point;
            anm_sincosmul(&point, angle, radius);
            *ANM_FLOAT_PTR(0) = point.x;
            *ANM_FLOAT_PTR(1) = point.y;
            break;
        }
        case ANM_OP_IJE:
            if (ANM_INT(0) == ANM_INT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_FJE:
            if (ANM_FLOAT(0) == ANM_FLOAT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_IJNE:
            if (ANM_INT(0) != ANM_INT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_FJNE:
            if (ANM_FLOAT(0) != ANM_FLOAT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_IJL:
            if (ANM_INT(0) < ANM_INT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_FJL:
            if (ANM_FLOAT(0) < ANM_FLOAT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_IJLE:
            if (ANM_INT(0) <= ANM_INT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_FJLE:
            if (ANM_FLOAT(0) <= ANM_FLOAT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_IJG:
            if (ANM_INT(0) > ANM_INT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_FJG:
            if (ANM_FLOAT(0) > ANM_FLOAT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_IJGE:
            if (ANM_INT(0) >= ANM_INT(1))
            {
                goto jump;
            }
            break;
        case ANM_OP_FJGE:
            if (ANM_FLOAT(0) >= ANM_FLOAT(1))
            {
                goto jump;
            }
            break;
        // The jump of ije ... fjge.
        jump:
            script_time.set_value(ins->args[3].i);
            instr_offset = ins->args[2].i;
            continue;
        case ANM_OP_SPRITE:
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
                g_AsciiManager->ascii_anm->set_sprite(this, ASCII_SPRITE_FALLBACK);
            }
            else
            {
                g_AnmManager->loaded_anms[anm_loaded_index]->set_sprite(this, sprite);
            }
            time_of_last_sprite_set = script_time.current;
            break;
        }
        case ANM_OP_IGNORE_GAME_SPEED:
            ANM_FLAGS_HI->ignore_game_speed = ANM_INT(0);
            break;
        case ANM_OP_SCRIPT_NEW:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this, ANM_CREATE_WORLD_BACK);
            break;
        case ANM_OP_SCRIPT_NEW_POS:
        {
            AnmId id = g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this,
                                                                                         ANM_CREATE_WORLD_BACK);
            AnmVm *child = id.find_or_clear();
            child->pos_2.x = ANM_FLOAT(1);
            child->pos_2.y = ANM_FLOAT(2);
            break;
        }
        case ANM_OP_SCRIPT_NEW_FRONT:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this, ANM_CREATE_FRONT);
            break;
        case ANM_OP_SCRIPT_NEW_UI:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this, ANM_CREATE_UI);
            break;
        case ANM_OP_SCRIPT_NEW_UI_FRONT:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_child(ANM_INT(0), this, ANM_CREATE_UI_FRONT);
            break;
        case ANM_OP_COPY_VARS:
            if (parent_vm != NULL)
            {
                memcpy(int_vars, parent_vm->int_vars, offsetof(AnmVm, pos_2) - offsetof(AnmVm, int_vars));
            }
            break;
        case ANM_OP_SCRIPT_NEW_ROOT_POS:
        {
            AnmId id = g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_root(ANM_INT(0), this, 0);
            AnmVm *child = id.find_or_clear();
            child->pos_2.x = ANM_FLOAT(1);
            child->pos_2.y = ANM_FLOAT(2);
            break;
        }
        case ANM_OP_SCRIPT_NEW_ROOT:
            g_AnmManager->loaded_anms[anm_loaded_index]->create_managed_root(ANM_INT(0), this, 0);
            break;
        case ANM_OP_EFFECT_NEW:
            g_EffectManager->create_effect(ANM_INT(0), (D3DXVECTOR3 *)this, this);
            break;
        case ANM_OP_SPRITE_RAND:
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
                g_AsciiManager->ascii_anm->set_sprite(this, ASCII_SPRITE_FALLBACK);
            }
            else
            {
                g_AnmManager->loaded_anms[anm_loaded_index]->set_sprite(this, sprite);
            }
            time_of_last_sprite_set = script_time.current;
            break;
        }
        case ANM_OP_SCALE:
            scale.x = ANM_FLOAT(0);
            scale.y = ANM_FLOAT(1);
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        case ANM_OP_SCALE2:
            scale_2.x = ANM_FLOAT(0);
            scale_2.y = ANM_FLOAT(1);
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        case ANM_OP_ZOOM_OUT:
            uv_scale.x = ANM_FLOAT(0);
            uv_scale.y = ANM_FLOAT(1);
            flags_lo |= ANM_VM_UV_SCALE_CHANGED;
            break;
        case ANM_OP_ALPHA:
            color_1.a = ANM_INT(0);
            break;
        case ANM_OP_COLOR:
            color_1.r = ANM_INT(0);
            color_1.g = ANM_INT(1);
            color_1.b = ANM_INT(2);
            break;
        case ANM_OP_ALPHA2:
            color_2.a = ANM_INT(0);
            break;
        case ANM_OP_COLOR2:
            color_2.r = ANM_INT(0);
            color_2.g = ANM_INT(1);
            color_2.b = ANM_INT(2);
            break;
        case ANM_OP_FLIP_X:
            flags_lo ^= ANM_VM_FLIP_X;
            scale.x *= -1.0f;
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        case ANM_OP_FLIP_Y:
            flags_lo ^= ANM_VM_FLIP_Y;
            scale.y *= -1.0f;
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        case ANM_OP_COLORIZE_CHILDREN:
            ANM_FLAGS_HI->colorize_children = (u8)ins->args[0].i;
            break;
        case ANM_OP_SHOW:
            flags_lo |= ANM_VM_SHOWN;
            break;
        case ANM_OP_HIDE:
            flags_lo &= ~ANM_VM_SHOWN;
            break;
        case ANM_OP_ROTATE:
            rotation.x = ANM_FLOAT(0);
            rotation.y = ANM_FLOAT(1);
            rotation.z = ANM_FLOAT(2);
            flags_lo |= ANM_VM_ROTATION_CHANGED;
            break;
        case ANM_OP_ANGLE_VEL:
            set_angular_velocity(ANM_FLOAT(0), ANM_FLOAT(1), ANM_FLOAT(2));
            break;
        case ANM_OP_SCALE_GROWTH:
            set_scale_growth(ANM_FLOAT(0), ANM_FLOAT(1));
            break;
        case ANM_OP_ALPHA_TIME_LINEAR:
            set_alpha1_time(ANM_INT(1), INTERP_LINEAR, color_1.a, (u8)ins->args[0].i);
            break;
        case ANM_OP_BLEND_MODE:
            ANM_FLAGS_LO->blend_mode = ins->args[0].i;
            break;
        case ANM_OP_POS:
            if (!(flags_lo & ANM_VM_POS_I_TO_POS_2))
            {
                pos = Float3(ANM_FLOAT(0), ANM_FLOAT(1), ANM_FLOAT(2));
            }
            else
            {
                pos_2 = Float3(ANM_FLOAT(0), ANM_FLOAT(1), ANM_FLOAT(2));
            }
            break;
        case ANM_OP_ANCHOR_OFFSET:
            anchor_offset.x = ANM_FLOAT(0);
            anchor_offset.y = ANM_FLOAT(1);
            break;
        case ANM_OP_ROTATION_MODE:
            ANM_FLAGS_HI->rotation_mode = ANM_INT(0);
            break;
        case ANM_OP_VISIBLE:
            ANM_FLAGS_LO->visible = ins->args[0].i;
            break;
        case ANM_OP_ANCHOR:
            ANM_FLAGS_LO->anchor_x = ((u16 *)ins->args)[0];
            ANM_FLAGS_LO->anchor_y = ((u16 *)ins->args)[1];
            break;
        case ANM_OP_SCROLL_X:
            uv_scroll_vel.x = ANM_FLOAT(0);
            flags_hi |= ANM_VM_HAS_VELOCITY;
            break;
        case ANM_OP_SCROLL_Y:
            uv_scroll_vel.y = ANM_FLOAT(0);
            flags_hi |= ANM_VM_HAS_VELOCITY;
            break;
        case ANM_OP_Z_WRITE_DISABLE:
            ANM_FLAGS_LO->z_write_disable = ins->args[0].i;
            break;
        case ANM_OP_FOLLOW_CAMERA:
            ANM_FLAGS_LO->follow_camera = ins->args[0].i;
            break;
        case ANM_OP_RESAMPLE_MODE:
            ANM_FLAGS_HI->filter_point = ins->args[0].i;
            break;
        case ANM_OP_POS_TIME:
            pos_i.end_time = ANM_INT(0);
            pos_i.bezier_1 = g_zero_vec;
            pos_i.bezier_2 = g_zero_vec;
            pos_i.method = ins->args[1].i;
            if (!(flags_lo & ANM_VM_POS_I_TO_POS_2))
            {
                pos_i.initial = pos;
            }
            else
            {
                pos_i.initial = pos_2;
            }
            pos_i.goal = Float3(ANM_FLOAT(2), ANM_FLOAT(3), ANM_FLOAT(4));
            pos_i.reset_timer();
            break;
        case ANM_OP_POS_TIME_POLAR:
        {
            pos_i.end_time = ANM_INT(0);
            pos_i.bezier_1 = g_zero_vec;
            pos_i.bezier_2 = g_zero_vec;
            pos_i.method = ins->args[1].i;
            if (!(flags_lo & ANM_VM_POS_I_TO_POS_2))
            {
                pos_i.initial = pos;
            }
            else
            {
                pos_i.initial = pos_2;
            }
            Float3 goal;
            anm_sincosmul(&goal, ANM_FLOAT(2), ANM_FLOAT(3));
            goal.z = 0.0f;
            pos_i.goal = goal;
            pos_i.reset_timer();
            break;
        }
        case ANM_OP_MOVE_BEZIER:
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
            if (!(flags_lo & ANM_VM_POS_I_TO_POS_2))
            {
                pos_i.initial = pos;
            }
            else
            {
                pos_i.initial = pos_2;
            }
            pos_i.goal = Float3(ANM_FLOAT(4), ANM_FLOAT(5), ANM_FLOAT(6));
            pos_i.reset_timer();
            break;
        }
        case ANM_OP_COLOR_TIME:
        {
            AnmRgb initial;
            anm_rgb(&initial, color_1.r, color_1.g, color_1.b);
            AnmRgb goal;
            anm_rgb(&goal, ANM_INT(2), ANM_INT(3), ANM_INT(4));
            set_rgb1_time(ANM_INT(0), (u8)ins->args[1].i, (ZunColor *)&initial, (ZunColor *)&goal);
            break;
        }
        case ANM_OP_ALPHA_TIME:
            set_alpha1_time(ANM_INT(0), (u8)ins->args[1].i, color_1.a, ANM_INT(2));
            break;
        case ANM_OP_COLOR2_TIME:
        {
            AnmRgb initial;
            anm_rgb(&initial, color_2.r, color_2.g, color_2.b);
            AnmRgb goal;
            anm_rgb(&goal, ANM_INT(2), ANM_INT(3), ANM_INT(4));
            set_rgb2_time(ANM_INT(0), (u8)ins->args[1].i, (ZunColor *)&initial, (ZunColor *)&goal);
            break;
        }
        case ANM_OP_ALPHA2_TIME:
            set_alpha2_time(ANM_INT(0), (u8)ins->args[1].i, color_2.a, ANM_INT(2));
            break;
        case ANM_OP_ROTATE_TIME:
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
        case ANM_OP_ROTATE_TIME_2D:
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
        case ANM_OP_SCALE_TIME:
        {
            Float2 goal(ANM_FLOAT(2), ANM_FLOAT(3));
            set_scale_interp(ANM_INT(0), (u8)ins->args[1].i, &scale, &goal);
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        }
        // Starts from scale, not scale_2.
        case ANM_OP_SCALE2_TIME:
        {
            Float2 goal(ANM_FLOAT(2), ANM_FLOAT(3));
            set_scale_2_time(ANM_INT(0), (u8)ins->args[1].i, &scale, &goal);
            flags_lo |= ANM_VM_SCALE_CHANGED;
            break;
        }
        case ANM_OP_ZOOM_OUT_TIME:
        {
            Float2 goal(ANM_FLOAT(2), ANM_FLOAT(3));
            set_uv_scale_time(ANM_INT(0), (u8)ins->args[1].i, &uv_scale, &goal);
            flags_lo |= ANM_VM_UV_SCALE_CHANGED;
            break;
        }
        case ANM_OP_SCROLL_X_TIME:
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
        case ANM_OP_SCROLL_Y_TIME:
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
        case ANM_OP_TYPE:
            ANM_FLAGS_LO->render_mode = ins->args[0].i;
            if (ANM_FLAGS_LO->render_mode == ANM_RENDER_FAN)
            {
                anm_fan_init(this);
            }
            break;
        case ANM_OP_POS_FROM_ENTITY:
            pos = entity_pos;
            entity_pos.x = 0.0f;
            entity_pos.y = 0.0f;
            entity_pos.z = 0.0f;
            break;
        case ANM_OP_TEX_CIRCLE:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_TEX_CIRCLE;
            alloc_extra_data(ANM_INT(0) * ANM_SCREEN_SEGMENT_SIZE);
            break;
        case ANM_OP_TEX_ARC_EVEN:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_TEX_ARC_EVEN;
            alloc_extra_data(ANM_INT(0) * ANM_SCREEN_SEGMENT_SIZE);
            break;
        case ANM_OP_TEX_ARC:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_TEX_ARC;
            alloc_extra_data(ANM_INT(0) * ANM_SCREEN_SEGMENT_SIZE);
            break;
        case ANM_OP_TEX_CYLINDER_3D:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_TEX_CYLINDER_3D;
            alloc_extra_data(ANM_INT(0) * ANM_3D_SEGMENT_SIZE);
            break;
        case ANM_OP_TEX_RING_3D:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_TEX_RING_3D;
            alloc_extra_data(ANM_INT(0) * ANM_3D_SEGMENT_SIZE);
            break;
        case ANM_OP_UV_FROM_CORNERS:
        {
            Float3 corners[4];
            write_sprite_corners(corners);
            divide_vec2_by_640_480(&uv_quad_of_sprite[0], (Float2 *)&corners[0]);
            divide_vec2_by_640_480(&uv_quad_of_sprite[1], (Float2 *)&corners[1]);
            divide_vec2_by_640_480(&uv_quad_of_sprite[2], (Float2 *)&corners[2]);
            divide_vec2_by_640_480(&uv_quad_of_sprite[3], (Float2 *)&corners[3]);
            break;
        }
        case ANM_OP_UV_FROM_CORNERS_ALWAYS:
            ANM_FLAGS_HI->uv_quad_from_corners = ANM_INT(0);
            break;
        case ANM_OP_DRAW_RECT:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_RECT;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case ANM_OP_DRAW_RECT_GRAD:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_RECT_GRAD;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case ANM_OP_DRAW_RECT_ROT:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_RECT_ROT;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case ANM_OP_DRAW_RECT_ROT_GRAD:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_RECT_ROT_GRAD;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case ANM_OP_DRAW_LINE:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_LINE;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case ANM_OP_DRAW_RECT_BORDER:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_RECT_BORDER;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            break;
        case ANM_OP_DRAW_POLY:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_POLY;
            sprite_size.x = ANM_FLOAT(0);
            int_vars[0] = ANM_INT(1);
            break;
        case ANM_OP_DRAW_POLY_BORDER:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_POLY_BORDER;
            sprite_size.x = ANM_FLOAT(0);
            int_vars[0] = ANM_INT(1);
            break;
        case ANM_OP_DRAW_RING:
            ANM_FLAGS_LO->render_mode = ANM_RENDER_RING;
            sprite_size.x = ANM_FLOAT(0);
            sprite_size.y = ANM_FLOAT(1);
            int_vars[0] = ANM_INT(2);
            break;
        case ANM_OP_NO_PARENT_POS:
            ANM_FLAGS_HI->no_parent_pos = ANM_INT(0);
            break;
        case ANM_OP_SCROLL_MODE:
            ANM_FLAGS_HI->address_u = ANM_INT(0);
            ANM_FLAGS_LO->address_v = ANM_INT(1);
            break;
        case ANM_OP_RESOLUTION_MODE:
            ANM_FLAGS_HI->resolution_mode = ANM_INT(0);
            break;
        case ANM_OP_ROTATE_WITH_PARENT:
            ANM_FLAGS_HI->rotate_with_parent = ANM_INT(0);
            break;
        case ANM_OP_ORIGIN_MODE:
            ANM_FLAGS_HI->origin_mode = (u8)ins->args[0].i;
            break;
        case ANM_OP_431:
            ANM_FLAGS_HI->ins_431_flag = (u8)ins->args[0].i;
            break;
        case ANM_OP_LAYER:
            set_layer((u8)ins->args[0].i);
            break;
        case ANM_OP_COLOR_MODE:
            ANM_FLAGS_LO->color_mode = (u8)ins->args[0].i;
            break;
        case ANM_OP_ROTATE_AUTO:
            ANM_FLAGS_HI->auto_rotate = (u8)ins->args[0].i;
            break;
        case ANM_OP_RAND_MODE:
            ANM_FLAGS_HI->rand_mode = (u8)ins->args[0].i;
            break;
        // stopHide is stop that also hides the VM.
        case ANM_OP_STOP_HIDE:
            flags_lo &= ~ANM_VM_VISIBLE;
        case ANM_OP_STOP:
            if (pending_interrupt != 0)
            {
                goto interrupt;
            }
            flags_lo |= ANM_VM_STOPPED;
            goto stop;
        // The end of the script deletes the VM too.
        case ANM_OP_END:
        case ANM_OP_DELETE:
            flags_lo &= ~ANM_VM_VISIBLE;
            result = 1;
        // delete falls through: both end the script.
        case ANM_OP_STATIC:
            instr_offset = -1;
            return result;
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
        return 1;
    }
    script_time.tick_split();
    return 0;
}

// Steps the VM by one frame, at the game speed scaled down by the
// slowdown of the VM (or its root). 1 once the VM should be deleted.
// TODO: same cases and layout; differs in: 106/112 keep the store pointer in ecx (original eax), 121 stores the fmod result before the pointer test so 123 cross-jumps into its tail, 130/131 register use (the lerp multiplies t from memory), the GameThread check's branch sense, case 2 clearing eax itself instead of jumping to the final return 0, and the camera add's operand order (y, z).
// FUNCTION: TH16 0x45f980
i32 AnmVm::run()
{
    f32 saved_game_speed = g_game_speed;
    if (flags_hi & ANM_VM_IGNORE_GAME_SPEED)
    {
        g_game_speed = 1.0f;
    }
    if (get_slowdown_factor_inline() > 0.0f)
    {
        g_game_speed = saved_game_speed - get_slowdown_factor_inline() * saved_game_speed;
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
    i32 result = run_script();
    g_game_speed = saved_game_speed;
    return result;
}

// FUNCTION: TH16 0x464dd0
Float3 *AnmVm::get_total_rotation()
{
    total_rotation = rotation;
    if (root_vm != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
    {
        total_rotation += *root_vm->get_total_rotation();
        rotation.x = wrap_angle(rotation.x);
        rotation.y = wrap_angle(rotation.y);
        rotation.z = wrap_angle(rotation.z);
    }
    return &total_rotation;
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
void AnmVm::set_scale_2_time(i32 end_time, i32 method, Float2 *initial, Float2 *goal)
{
    scale_2_i.end_time = end_time;
    scale_2_i.method = method;
    scale_2_i.initial = *initial;
    scale_2_i.goal = *goal;
    scale_2_i.time = 0;
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

// HARNESS_CALLED: with AnmVm::run as its only caller LTCG knows its stack is
// 8-aligned, so it does not realign its frame (the original does not).
// FUNCTION: TH16 0x463b30
HARNESS_CALLED void AnmVm::step_interpolators()
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
    if (scale_2_i.end_time != 0)
    {
        scale_2 = scale_2_i.step();
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
