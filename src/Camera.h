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
};

// One of the Supervisor's four cameras. Layout from ExpHP's th-re-data.
struct Camera
{
    Float3 position;
    Float3 facing;
    Float3 up;
    Float3 facing_normalized;
    Float3 unk_30;
    Float3 rocking_vector_1;
    Float3 rocking_vector_2;
    f32 field_of_view;
    i32 window_resolution[2];
    D3DMATRIX view_matrix;
    D3DMATRIX projection_matrix;
    D3DVIEWPORT9 viewport;
    i32 camera_index;
    Float2 unk_fc;
    Float3 unk_104;
    CameraSky sky;

    // 0x40d510. Empty; g_Supervisor's static initializer calls it for each
    // camera.
    Camera();
};

// 0x43c780. Recomputes a camera's matrices for a flat view of its viewport.
void __stdcall camera_update_43c780(Camera *camera);
// 0x43c940. Recomputes a camera's matrices from its position, rocking and
// facing, and makes them the device's.
void __stdcall camera_apply_43c940(Camera *camera);
