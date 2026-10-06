#pragma once

#include "ZunMath.h"
#include "decomp.h"
#include "types.h"

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

// Progress (usually 0 to 1) of an interpolation `time` frames into one of
// `end_time` frames.
HARNESS_CALLED f32 interp_ratio(i32 mode, f32 time, f32 end_time);
