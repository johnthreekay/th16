#pragma once

#include <d3dx9math.h>

#include "ZunTimer.h"
#include "types.h"

// Interpolators (ExpHP: zInterpFloat, zInterpFloat2, zInterpStrange1).
// The constructors only construct the timer; owners clear end_time.
struct InterpFloat
{
    f32 initial;
    f32 goal;
    f32 bezier_1;
    f32 bezier_2;
    f32 current;
    ZunTimer time;
    i32 end_time;
    i32 method;
};

struct InterpFloat2
{
    D3DXVECTOR2 initial;
    D3DXVECTOR2 goal;
    D3DXVECTOR2 bezier_1;
    D3DXVECTOR2 bezier_2;
    D3DXVECTOR2 current;
    ZunTimer time;
    i32 end_time;
    i32 method;

    void reset_timer();
};

struct InterpStrange1
{
    D3DXVECTOR3 current;
    D3DXVECTOR3 initial;
    D3DXVECTOR3 goal;
    D3DXVECTOR3 bezier_1;
    D3DXVECTOR3 bezier_2;
    ZunTimer time;
    i32 end_time;
    i32 method_for_1d;
    u32 move_curve_mode;
    u32 unk_5c;
    i32 method_for_3d;
    i32 flag_1d;

    void reset_timer();
};
