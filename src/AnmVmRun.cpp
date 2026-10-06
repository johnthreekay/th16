#include "AnmManager.h"
#include "AnmVm.h"
#include "Rng.h"
#include "Supervisor.h"

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
        return g_ReplayUnsafeRng.randf_0_to(rand_scale_one);
    case ANM_VAR_RANDF2_UNSAFE:
        return g_ReplayUnsafeRng.randf_neg_to(rand_scale_one);
    case ANM_VAR_RANDRAD_UNSAFE:
        return g_ReplayUnsafeRng.randf_neg_to(rand_scale_pi);
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
        return g_ReplayUnsafeRng.rand_u32();
    case ANM_VAR_RANDF_SAFE:
        return g_ReplaySafeRng.randf_0_to(rand_scale_one);
    case ANM_VAR_RANDF2_SAFE:
        return g_ReplaySafeRng.randf_neg_to(rand_scale_one);
    case ANM_VAR_RANDRAD_SAFE:
        return g_ReplaySafeRng.randf_neg_to(rand_scale_pi);
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
        return g_ReplayUnsafeRng.rand_u32_in_range(num_cycles_in_texture);
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
