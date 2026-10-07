#pragma once

#include <d3dx9math.h>

#include "ZunAngle.h"
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

    // Starts an interpolation from initial to goal over end_time frames
    // (ECL's move*Time instructions).
    void start(i32 end_time, i32 method, f32 initial, f32 goal)
    {
        this->initial = initial;
        this->end_time = end_time;
        this->method = method;
        bezier_1 = 0.0f;
        bezier_2 = 0.0f;
        this->goal = goal;
        reset();
    }
};

// A zero vector that the ECL radial and ellipse interpolations copy their
// bezier values from.
extern D3DXVECTOR2 g_zero_vec2;

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
    D3DXVECTOR2 step();
    // Starts an interpolation from initial to goal over end_time frames
    // (ECL's moveCircleTime and moveEllipseTime).
    void start(i32 end_time, i32 method, D3DXVECTOR2 *initial, D3DXVECTOR2 *goal)
    {
        this->end_time = end_time;
        bezier_1 = g_zero_vec2;
        bezier_2 = g_zero_vec2;
        this->method = method;
        this->initial = *initial;
        this->goal = *goal;
        reset_timer();
    }
    // 0x425570. A second copy of step that the enemies' radial distance
    // interpolators use (ExpHP: InterpRadialDist::step).
    D3DXVECTOR2 step_radial_dist();
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

    // 0x406200
    void reset_timer();
    // 0x406e10
    D3DXVECTOR3 step();
};

// An InterpFloat whose values are angles kept in [-pi, pi] (ExpHP:
// zInterpFloat, used for AnmVm::rotate_2d_i).
struct InterpAngle
{
    ZunAngle initial;
    ZunAngle goal;
    ZunAngle bezier_1;
    ZunAngle bezier_2;
    ZunAngle current;
    ZunTimer time;
    i32 end_time;
    i32 method;

    // The same code as InterpFloat::reset, but a separate function.
    void reset_time();
    ZunAngle step();
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

    i32 step();
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

    Int3 step();
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
    // With flag_1d bit 0, each axis interpolates on its own with these
    // three methods (x, y, z); otherwise method_for_3d applies to all.
    union
    {
        struct
        {
            i32 method_for_1d;
            u32 move_curve_mode;
            u32 unk_5c;
        };
        i32 methods_1d[3];
    };
    i32 method_for_3d;
    i32 flag_1d;

    void reset_timer();
    // 0x4258b0
    D3DXVECTOR3 step();
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
