#include "Interp.h"

// FUNCTION: TH16 0x417180
void InterpFloat::reset()
{
    time.reset();
}

// TODO: case 17 copies initial to current as an integer and returns current
// reloaded.
// FUNCTION: TH16 0x4171c0
HARNESS_CALLED f32 InterpFloat::step()
{
    if (end_time > 0)
    {
        time.tick();
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

// FUNCTION: TH16 0x425530
void InterpFloat2::reset_timer()
{
    time = 0;
}

// FUNCTION: TH16 0x425870
void InterpStrange1::reset_timer()
{
    time = 0;
}
