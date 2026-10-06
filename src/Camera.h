#pragma once

#include <d3d9.h>

#include "ZunMath.h"
#include "types.h"

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
    u8 sky[0x12c - 0x110];
};
