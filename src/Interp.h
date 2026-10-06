#pragma once

#include <d3dx9math.h>

#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// Interpolated values: initial and goal plus two bezier control values,
// stepped by a timer. Layouts from ExpHP (zInterpFloat, zInterpFloat2,
// zInterpFloat3, zInterpInt, zInterpInt3, zInterpStrange1). The
// constructors only construct the timer; owners clear end_time.
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

    DECOMP_NOINLINE void reset();
    HARNESS_CALLED f32 step();
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

struct InterpFloat3
{
    D3DXVECTOR3 initial;
    D3DXVECTOR3 goal;
    D3DXVECTOR3 bezier_1;
    D3DXVECTOR3 bezier_2;
    D3DXVECTOR3 current;
    ZunTimer time;
    i32 end_time;
    i32 method;
};

struct InterpInt
{
    i32 initial;
    i32 goal;
    i32 bezier_1;
    i32 bezier_2;
    i32 current;
    ZunTimer time;
    i32 end_time;
    i32 method;
};

struct InterpInt3
{
    i32 initial[3];
    i32 goal[3];
    i32 bezier_1[3];
    i32 bezier_2[3];
    i32 current[3];
    ZunTimer time;
    i32 end_time;
    i32 method;
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

// The shared interpolation curves (linear, ease in/out, ...): the fraction
// of the way from initial to goal at time t of end_time.
HARNESS_CALLED f32 interp_common_methods(i32 method, f32 t, f32 end_time);
