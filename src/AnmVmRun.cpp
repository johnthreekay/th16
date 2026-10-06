#include <stddef.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "Rng.h"
#include "Supervisor.h"

static_assert(offsetof(AnmVm, rotation_related) == 0x5f0, "AnmVm layout");
static_assert(sizeof(AnmVm) == 0x5fc, "AnmVm layout");
static_assert(sizeof(InterpInt3) == 0x58, "InterpInt3 layout");
static_assert(sizeof(InterpAngle) == 0x30, "InterpAngle layout");

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
i32 AnmVm::get_int_var(i32 value)
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
f32 *AnmVm::get_float_var_ptr(f32 *value)
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
i32 *AnmVm::get_int_var_ptr(i32 *value)
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
