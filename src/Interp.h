#pragma once

#include <d3dx9math.h>

#include "ZunMath.h"
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

// Three ints, copied as a whole (ExpHP: zInt3). Indexing keeps the
// array syntax InterpInt3's fields had before.
struct Int3
{
    i32 x;
    i32 y;
    i32 z;

    Int3()
    {
    }

    Int3(i32 x, i32 y, i32 z)
    {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    i32 &operator[](i32 i)
    {
        return (&x)[i];
    }
};

struct InterpInt3
{
    Int3 initial;
    Int3 goal;
    Int3 bezier_1;
    Int3 bezier_2;
    Int3 current;
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

// Easing curves of ANM/ECL interpolation (the "mode" of an interpolator).
enum InterpMode
{
    INTERP_LINEAR = 0,
    INTERP_EASE_IN_2 = 1,
    INTERP_EASE_IN_3 = 2,
    INTERP_EASE_IN_4 = 3,
    INTERP_EASE_OUT_2 = 4,
    INTERP_EASE_OUT_3 = 5,
    INTERP_EASE_OUT_4 = 6,
    // Adds the goal to the value every frame instead.
    INTERP_CONSTANT_VELOCITY = 7,
    INTERP_BEZIER = 8,
    INTERP_EASE_IN_OUT_2 = 9,
    INTERP_EASE_IN_OUT_3 = 10,
    INTERP_EASE_IN_OUT_4 = 11,
    INTERP_EASE_OUT_IN_2 = 12,
    INTERP_EASE_OUT_IN_3 = 13,
    INTERP_EASE_OUT_IN_4 = 14,
    INTERP_FORCE_INITIAL = 15,
    INTERP_FORCE_FINAL = 16,
    // Adds the bezier_1 delta to the value every frame.
    INTERP_CONSTANT_ACCEL = 17,
    INTERP_EASE_OUT_SINE = 18,
    INTERP_EASE_IN_SINE = 19,
    INTERP_EASE_IN_OUT_SINE = 20,
    INTERP_EASE_OUT_IN_SINE = 21,
    // Back up a little before moving on (the four next ones further).
    INTERP_EASE_IN_BACK_A = 22,
    INTERP_EASE_IN_BACK_B = 23,
    INTERP_EASE_IN_BACK_C = 24,
    INTERP_EASE_IN_BACK_D = 25,
    INTERP_EASE_IN_BACK_E = 26,
    // Overshoot the goal a little and come back.
    INTERP_EASE_OUT_BACK_A = 27,
    INTERP_EASE_OUT_BACK_B = 28,
    INTERP_EASE_OUT_BACK_C = 29,
    INTERP_EASE_OUT_BACK_D = 30,
    INTERP_EASE_OUT_BACK_E = 31,
};

// The shared interpolation curves (InterpMode): progress (usually 0 to 1)
// of an interpolation `time` frames into one of `end_time` frames.
HARNESS_CALLED f32 interp_common_methods(i32 mode, f32 time, f32 end_time);
