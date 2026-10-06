#include <math.h>

#include "AnmVm.h"
#include "ZunMath.h"

// STUB: TH16 0x4033f0
// Placeholder (the real function is outside unit 3), compiled with /GL and
// kept out of line so that LTCG gives it the original's register convention:
// method in ecx, t in xmm1, end_time in xmm2. The original has some thirty
// curves.
HARNESS_CALLED f32 interp_common_methods(i32 method, f32 t, f32 end_time)
{
    if (end_time == 0.0f)
    {
        return 1.0f;
    }
    t /= end_time;
    switch (method)
    {
    case 1:
        return t * t;
    case 2:
        return t * t * t;
    case 4:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case 9:
        // Some of the real curves call into the CRT, which matters to
        // the callers' register allocation.
        return sinf(t * ZUN_PI / 2);
    }
    return t;
}

// FUNCTION: TH16 0x417180
void InterpFloat::reset()
{
    time.reset();
}

// TODO: eax and ecx swapped for end_time and method, and case 17 copies
// initial to current as an integer and returns current reloaded.
// FUNCTION: TH16 0x4171c0
HARNESS_CALLED f32 InterpFloat::step()
{
    if (end_time > 0)
    {
        time++;
        if (time.current >= end_time)
        {
            time.set(end_time);
            end_time = 0;
            if (method == 7 || method == 17)
            {
                return initial;
            }
            return goal;
        }
    }
    else if (end_time == 0)
    {
        if (method == 7 || method == 17)
        {
            return initial;
        }
        return goal;
    }
    if (method == 7)
    {
        // Constant velocity: goal is the step.
        initial += goal;
        current = initial;
        return current;
    }
    else if (method == 17)
    {
        // Constant acceleration: goal is added to the step.
        initial += bezier_2;
        bezier_2 += goal;
        current = initial;
        return current;
    }
    else if (method == 8)
    {
        // Cubic Hermite curve; the bezier fields are the tangents.
        f32 t = time.current_f / (f32)end_time;
        current = t * t * (3.0f - 2.0f * t) * goal + (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f) * initial +
                  (1.0f - t) * (1.0f - t) * t * bezier_1 + (t - 1.0f) * t * t * bezier_2;
        return current;
    }
    current = interp_common_methods(method, time.current_f, (f32)end_time) * (goal - initial) + initial;
    return current;
}
