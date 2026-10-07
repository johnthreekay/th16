#pragma once

#include <d3d9.h>

#include "ZunMath.h"
#include "decomp.h"
#include "types.h"

// Distance fog of a camera: linear from begin to end distance, in color.
// The float components are what interpolation works on; color follows
// them. Layout from ExpHP (zCameraSky).
struct CameraSky
{
    f32 begin_distance;
    f32 end_distance;
    f32 color_components[4];
    u8 color[4];

    CameraSky() = default;
    // 0x40d400
    HARNESS_CALLED CameraSky(f32 begin_distance, f32 end_distance, f32 c0, f32 c1, f32 c2, f32 c3);
    // 0x40d370
    HARNESS_CALLED CameraSky operator+(const CameraSky &other) const;

    // operator+ as InterpCameraSky::step has it inline (method 7).
    __forceinline CameraSky add_inline(const CameraSky &other) const
    {
        CameraSky result;
        result.begin_distance = begin_distance + other.begin_distance;
        result.end_distance = end_distance + other.end_distance;
        result.color_components[0] = color_components[0] + other.color_components[0];
        result.color_components[1] = color_components[1] + other.color_components[1];
        result.color_components[2] = color_components[2] + other.color_components[2];
        result.color_components[3] = color_components[3] + other.color_components[3];
        for (i32 i = 0; i < 4; i++)
        {
            result.color[i] = color_components[i] + other.color_components[i];
        }
        return result;
    }

    CameraSky operator-(const CameraSky &other) const
    {
        CameraSky result;
        result.begin_distance = begin_distance - other.begin_distance;
        result.end_distance = end_distance - other.end_distance;
        result.color_components[0] = color_components[0] - other.color_components[0];
        result.color_components[1] = color_components[1] - other.color_components[1];
        result.color_components[2] = color_components[2] - other.color_components[2];
        result.color_components[3] = color_components[3] - other.color_components[3];
        for (i32 i = 0; i < 4; i++)
        {
            result.color[i] = color_components[i] - other.color_components[i];
        }
        return result;
    }

    CameraSky operator*(f32 s) const
    {
        CameraSky result;
        result.begin_distance = begin_distance * s;
        result.end_distance = end_distance * s;
        result.color_components[0] = color_components[0] * s;
        result.color_components[1] = color_components[1] * s;
        result.color_components[2] = color_components[2] * s;
        result.color_components[3] = color_components[3] * s;
        for (i32 i = 0; i < 4; i++)
        {
            result.color[i] = color_components[i] * s;
        }
        return result;
    }
};

// One of the Supervisor's four cameras (camera 3 is the stage's 3D view,
// camera 2 the whole window). Layout from ExpHP's th-re-data.
struct Camera
{
    Float3 position;
    Float3 facing;
    Float3 up;
    Float3 facing_normalized;
    // facing x up, normalized (camera_apply_3d): projected to size
    // billboarded sprites.
    Float3 right;
    // Offsets of the eye (rocking_vector_1, added to position) and of the
    // facing (rocking_vector_2) from STD_ROCKING_MODE.
    Float3 rocking_vector_1;
    Float3 rocking_vector_2;
    f32 field_of_view;
    i32 window_resolution[2];
    D3DMATRIX view_matrix;
    D3DMATRIX projection_matrix;
    D3DVIEWPORT9 viewport;
    i32 camera_index;
    // The screen shake's offset (ScreenEffect), which the ANM manager adds
    // to sprites.
    Float2 shake_offset;
    // How far STD_POS moved the camera this frame; ANM instruction 306 has
    // VMs follow it.
    Float3 position_delta;
    CameraSky sky;

    // 0x40d510. Empty; g_Supervisor's static initializer calls it for each
    // camera.
    Camera();
};

// 0x43c780. Recomputes a camera's matrices for a flat view of its viewport.
void __stdcall camera_update_2d(Camera *camera);
// 0x43c940. Recomputes a camera's matrices from its position, rocking and
// facing, and makes them the device's; also updates right.
void __stdcall camera_apply_3d(Camera *camera);
