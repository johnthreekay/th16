#include <math.h>

#include "Interp.h"

// A parabola through (a, 0) rescaled to run from 0 to 1, so it dips below 0
// first.
static inline f32 ease_in_back(f32 x, f32 a)
{
    return ((x - a) * (x - a) / ((1.0f - a) * (1.0f - a)) - a * a / ((1.0f - a) * (1.0f - a))) /
           (1.0f - a * a / ((1.0f - a) * (1.0f - a)));
}

// TODO: register allocation differs in most curves; ours shares the 0.5f constant across branches.
// FUNCTION: TH16 0x4033f0
HARNESS_CALLED f32 interp_common_methods(i32 mode, f32 time, f32 end_time)
{
    if (end_time == 0.0f)
    {
        return 1.0f;
    }
    f32 x = time / end_time;
    switch (mode)
    {
    case INTERP_EASE_IN_2:
        return x * x;
    case INTERP_EASE_OUT_2:
        return 1.0f - (1.0f - x) * (1.0f - x);
    case INTERP_EASE_IN_3:
        return x * x * x;
    case INTERP_EASE_OUT_3:
        return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x);
    case INTERP_EASE_IN_4:
        return x * x * x * x;
    case INTERP_EASE_OUT_4:
        return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x) * (1.0f - x);
    case INTERP_EASE_IN_OUT_2:
        x *= 2.0f;
        if (x < 1.0f)
        {
            return x * x * 0.5f;
        }
        return (2.0f - (2.0f - x) * (2.0f - x)) * 0.5f;
    case INTERP_EASE_OUT_IN_2:
        x *= 2.0f;
        if (x < 1.0f)
        {
            return 0.5f - (1.0f - x) * (1.0f - x) * 0.5f;
        }
        return (x - 1.0f) * (x - 1.0f) * 0.5f + 0.5f;
    case INTERP_EASE_IN_OUT_3:
        x *= 2.0f;
        if (x < 1.0f)
        {
            return x * x * x * 0.5f;
        }
        return (2.0f - (2.0f - x) * (2.0f - x) * (2.0f - x)) * 0.5f;
    case INTERP_EASE_OUT_IN_3:
        x *= 2.0f;
        if (x < 1.0f)
        {
            return 0.5f - (1.0f - x) * (1.0f - x) * (1.0f - x) * 0.5f;
        }
        return (x - 1.0f) * (x - 1.0f) * (x - 1.0f) * 0.5f + 0.5f;
    case INTERP_EASE_IN_OUT_4:
        x *= 2.0f;
        if (x < 1.0f)
        {
            return x * x * x * x * 0.5f;
        }
        return (2.0f - (2.0f - x) * (2.0f - x) * (2.0f - x) * (2.0f - x)) * 0.5f;
    case INTERP_EASE_OUT_IN_4:
        x *= 2.0f;
        if (x < 1.0f)
        {
            return 0.5f - (1.0f - x) * (1.0f - x) * (1.0f - x) * (1.0f - x) * 0.5f;
        }
        return (x - 1.0f) * (x - 1.0f) * (x - 1.0f) * (x - 1.0f) * 0.5f + 0.5f;
    case INTERP_FORCE_INITIAL:
        return 0.0f;
    case INTERP_FORCE_FINAL:
        return 1.0f;
    case INTERP_EASE_OUT_SINE:
        return sinf(x * ZUN_PI * 0.5f);
    case INTERP_EASE_IN_SINE:
        return 1.0f - sinf(x * ZUN_PI * 0.5f + ZUN_PI / 2);
    case INTERP_EASE_IN_OUT_SINE:
        x *= 2.0f;
        if (x < 1.0f)
        {
            return sinf(x * ZUN_PI * 0.5f) * 0.5f;
        }
        return (1.0f - sinf(x * ZUN_PI * 0.5f)) * 0.5f + 0.5f;
    case INTERP_EASE_OUT_IN_SINE:
        x *= 2.0f;
        if (x < 1.0f)
        {
            return (1.0f - sinf(x * ZUN_PI * 0.5f + ZUN_PI / 2)) * 0.5f;
        }
        return sinf((x - 1.0f) * ZUN_PI * 0.5f) * 0.5f + 0.5f;
    case INTERP_EASE_IN_BACK_A:
        return ease_in_back(x, 0.25f);
    case INTERP_EASE_IN_BACK_B:
        return ease_in_back(x, 0.3f);
    case INTERP_EASE_IN_BACK_C:
        return ease_in_back(x, 0.35f);
    case INTERP_EASE_IN_BACK_D:
        return ease_in_back(x, 0.38f);
    case INTERP_EASE_IN_BACK_E:
        return ease_in_back(x, 0.4f);
    case INTERP_EASE_OUT_BACK_A:
        return 1.0f - ease_in_back(1.0f - x, 0.25f);
    case INTERP_EASE_OUT_BACK_B:
        return 1.0f - ease_in_back(1.0f - x, 0.3f);
    case INTERP_EASE_OUT_BACK_C:
        return 1.0f - ease_in_back(1.0f - x, 0.35f);
    case INTERP_EASE_OUT_BACK_D:
        return 1.0f - ease_in_back(1.0f - x, 0.38f);
    case INTERP_EASE_OUT_BACK_E:
        return 1.0f - ease_in_back(1.0f - x, 0.4f);
    }
    return x;
}

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
        bezier_2 = bezier_2 + goal;
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

// FUNCTION: TH16 0x406200
void InterpFloat3::reset_timer()
{
    time = 0;
}

// FUNCTION: TH16 0x425870
void InterpStrange1::reset_timer()
{
    time = 0;
}

// FUNCTION: TH16 0x4643b0
i32 InterpInt::step()
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
        initial += goal;
        current = initial;
        return current;
    }
    else if (method == 17)
    {
        initial += bezier_2;
        bezier_2 = bezier_2 + goal;
        current = initial;
        return current;
    }
    else if (method == 8)
    {
        f32 t = time.current_f / (f32)end_time;
        current = t * t * (3.0f - 2.0f * t) * goal + (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f) * initial +
                  (1.0f - t) * (1.0f - t) * t * bezier_1 + (t - 1.0f) * t * t * bezier_2;
        return current;
    }
    current = interp_common_methods(method, time.current_f, (f32)end_time) * (goal - initial) + initial;
    return current;
}

// TODO: ours aligns the frame (and esp, -8) and orders the bezier terms and the method 17 adds differently.
// FUNCTION: TH16 0x463d40
D3DXVECTOR2 InterpFloat2::step()
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
        D3DXVECTOR2 tmp = initial;
        initial = tmp + goal;
        current = initial;
    }
    else if (method == 17)
    {
        D3DXVECTOR2 tmp = initial;
        initial = bezier_2 + tmp;
        bezier_2 = bezier_2 + goal;
        current = initial;
    }
    else if (method == 8)
    {
        f32 t = time.current_f / (f32)end_time;
        f32 c_initial = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
        f32 c_goal = t * t * (3.0f - 2.0f * t);
        f32 c_bezier_1 = (1.0f - t) * (1.0f - t) * t;
        f32 c_bezier_2 = (t - 1.0f) * t * t;
        current = initial * c_initial + goal * c_goal + bezier_1 * c_bezier_1 + bezier_2 * c_bezier_2;
    }
    else
    {
        f32 x = interp_common_methods(method, time.current_f, (f32)end_time);
        current = (goal - initial) * x + initial;
    }
    return current;
}

// TODO: the timer tick and the bezier terms differ in register allocation,
// and method 17 loads goal.x before bezier_2.x.
// FUNCTION: TH16 0x406e10
D3DXVECTOR3 InterpFloat3::step()
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
        D3DXVECTOR3 tmp = initial;
        initial = goal + tmp;
        current = initial;
    }
    else if (method == 17)
    {
        D3DXVECTOR3 tmp = initial;
        initial = tmp + bezier_2;
        bezier_2 = bezier_2 + goal;
        current = initial;
    }
    else if (method == 8)
    {
        f32 t = time.current_f / (f32)end_time;
        f32 c_initial = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
        f32 c_goal = t * t * (3.0f - 2.0f * t);
        f32 c_bezier_1 = (1.0f - t) * (1.0f - t) * t;
        f32 c_bezier_2 = (t - 1.0f) * t * t;
        current = goal * c_goal + initial * c_initial + bezier_1 * c_bezier_1 + bezier_2 * c_bezier_2;
    }
    else
    {
        f32 x = interp_common_methods(method, time.current_f, (f32)end_time);
        current = (goal - initial) * x + initial;
    }
    return current;
}

// TODO: same as InterpFloat2::step, of which this is a second copy.
// FUNCTION: TH16 0x425570
D3DXVECTOR2 InterpFloat2::step_radial_dist()
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
        D3DXVECTOR2 tmp = initial;
        initial = tmp + goal;
        current = initial;
    }
    else if (method == 17)
    {
        D3DXVECTOR2 tmp = initial;
        initial = bezier_2 + tmp;
        bezier_2 = bezier_2 + goal;
        current = initial;
    }
    else if (method == 8)
    {
        f32 t = time.current_f / (f32)end_time;
        f32 c_initial = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
        f32 c_goal = t * t * (3.0f - 2.0f * t);
        f32 c_bezier_1 = (1.0f - t) * (1.0f - t) * t;
        f32 c_bezier_2 = (t - 1.0f) * t * t;
        current = initial * c_initial + goal * c_goal + bezier_1 * c_bezier_1 + bezier_2 * c_bezier_2;
    }
    else
    {
        f32 x = interp_common_methods(method, time.current_f, (f32)end_time);
        current = (goal - initial) * x + initial;
    }
    return current;
}

// TODO: some vector adds load their operands the other way round, and method 17 per axis keeps
// the sum in xmm0 where the original copies it back through eax.
// FUNCTION: TH16 0x4258b0
D3DXVECTOR3 InterpStrange1::step()
{
    if (end_time > 0)
    {
        time.tick();
        if (time.current >= end_time)
        {
            time.set(end_time);
            end_time = 0;
            if (method_for_3d == 7 || method_for_3d == 17)
            {
                return initial;
            }
            return goal;
        }
    }
    else if (end_time == 0)
    {
        if (method_for_3d == 7 || method_for_3d == 17)
        {
            return initial;
        }
        return goal;
    }
    if (!(flag_1d & 1))
    {
        if (method_for_3d == 7)
        {
            D3DXVECTOR3 tmp = initial;
            initial = goal + tmp;
            current = initial;
        }
        else if (method_for_3d == 17)
        {
            D3DXVECTOR3 tmp = initial;
            initial = bezier_2 + tmp;
            bezier_2 = bezier_2 + goal;
            current = initial;
        }
        else if (method_for_3d == 8)
        {
            f32 t = time.current_f / (f32)end_time;
            f32 c_initial = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
            f32 c_goal = t * t * (3.0f - 2.0f * t);
            f32 c_bezier_1 = (1.0f - t) * (1.0f - t) * t;
            f32 c_bezier_2 = (t - 1.0f) * t * t;
            current = initial * c_initial + goal * c_goal + bezier_1 * c_bezier_1 + bezier_2 * c_bezier_2;
        }
        else
        {
            f32 x = interp_common_methods(method_for_3d, time.current_f, (f32)end_time);
            current = (goal - initial) * x + initial;
        }
    }
    else
    {
        for (i32 i = 0; i < 3; i++)
        {
            if (methods_1d[i] == 7)
            {
                initial[i] = goal[i] + initial[i];
                current[i] = initial[i];
            }
            else if (methods_1d[i] == 17)
            {
                initial[i] = bezier_2[i] + initial[i];
                current[i] = initial[i];
                bezier_2[i] = bezier_2[i] + goal[i];
            }
            else if (methods_1d[i] == 8)
            {
                f32 t = time.current_f / (f32)end_time;
                current[i] = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f) * initial[i] +
                             t * t * (3.0f - 2.0f * t) * goal[i] + (1.0f - t) * (1.0f - t) * t * bezier_1[i] +
                             (t - 1.0f) * t * t * bezier_2[i];
            }
            else
            {
                f32 x = interp_common_methods(methods_1d[i], time.current_f, (f32)end_time);
                current[i] = (goal[i] - initial[i]) * x + initial[i];
            }
        }
    }
    return current;
}

// TODO: the timer tick adds current_f and the speed the other way round, and one lea swaps its operands.
// FUNCTION: TH16 0x464590
Int3 InterpInt3::step()
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
        Int3 tmp = initial;
        initial = tmp + goal;
        current = initial;
    }
    else if (method == 17)
    {
        Int3 tmp = initial;
        initial = tmp + bezier_2;
        bezier_2 = bezier_2 + goal;
        current = initial;
    }
    else if (method == 8)
    {
        f32 t = time.current_f / (f32)end_time;
        f32 c_initial = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
        f32 c_goal = t * t * (3.0f - 2.0f * t);
        f32 c_bezier_1 = (1.0f - t) * (1.0f - t) * t;
        f32 c_bezier_2 = (t - 1.0f) * t * t;
        current = initial * c_initial + goal * c_goal + bezier_1 * c_bezier_1 + bezier_2 * c_bezier_2;
    }
    else
    {
        f32 x = interp_common_methods(method, time.current_f, (f32)end_time);
        current = (goal - initial) * x + initial;
    }
    return current;
}

// FUNCTION: TH16 0x464040
void InterpAngle::reset_time()
{
    time.reset();
}
