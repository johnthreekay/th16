#include <math.h>

#include "Interp.h"

// A zero vector that is never written (ExpHP:
// SEEMINGLY_CONST_ZERO_VEC_4d9dc4); interpolators take their unused bezier
// control points from it, and bullet code passes it by address.
// GLOBAL: TH16 0x4d9dc4
Float3 g_zero_vec;

// A parabola through (a, 0) rescaled to run from 0 to 1, so it dips below 0
// first.
static inline f32 ease_in_back(f32 x, f32 a)
{
    return ((x - a) * (x - a) / ((1.0f - a) * (1.0f - a)) - a * a / ((1.0f - a) * (1.0f - a))) /
           (1.0f - a * a / ((1.0f - a) * (1.0f - a)));
}

// TODO: (reccmp 86%) most two-branch curves assign x and return it once, which the original's
// registers show; in-out-2 returns from its else branch, which keeps 2.0f in xmm1 like the original.
// Left: in-out-2's result is not moved back into xmm3 before the multiply, out-in-sine (original
// result in xmm1, 0.5f loaded once in the else), and reccmp cannot name the original's addresses of
// the constants only SSE code uses (the ease-back divisors), which count as differences.
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
            x = x * x;
        }
        else
        {
            return (2.0f - (2.0f - x) * (2.0f - x)) * 0.5f;
        }
        return x * 0.5f;
    case INTERP_EASE_OUT_IN_2:
        x *= 2.0f;
        if (x < 1.0f)
        {
            x = 0.5f - (1.0f - x) * (1.0f - x) * 0.5f;
        }
        else
        {
            x = (x - 1.0f) * (x - 1.0f) * 0.5f + 0.5f;
        }
        return x;
    case INTERP_EASE_IN_OUT_3:
        x *= 2.0f;
        if (x < 1.0f)
        {
            x = x * x * x;
        }
        else
        {
            x = 2.0f - (2.0f - x) * (2.0f - x) * (2.0f - x);
        }
        return x * 0.5f;
    case INTERP_EASE_OUT_IN_3:
        x *= 2.0f;
        if (x < 1.0f)
        {
            x = 0.5f - (1.0f - x) * (1.0f - x) * (1.0f - x) * 0.5f;
        }
        else
        {
            x = (x - 1.0f) * (x - 1.0f) * (x - 1.0f) * 0.5f + 0.5f;
        }
        return x;
    case INTERP_EASE_IN_OUT_4:
        x *= 2.0f;
        if (x < 1.0f)
        {
            x = x * x * x * x;
        }
        else
        {
            x = 2.0f - (2.0f - x) * (2.0f - x) * (2.0f - x) * (2.0f - x);
        }
        return x * 0.5f;
    case INTERP_EASE_OUT_IN_4:
        x *= 2.0f;
        if (x < 1.0f)
        {
            x = 0.5f - (1.0f - x) * (1.0f - x) * (1.0f - x) * (1.0f - x) * 0.5f;
        }
        else
        {
            x = (x - 1.0f) * (x - 1.0f) * (x - 1.0f) * (x - 1.0f) * 0.5f + 0.5f;
        }
        return x;
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
            x = (1.0f - sinf(x * ZUN_PI * 0.5f + ZUN_PI / 2)) * 0.5f;
        }
        else
        {
            x = sinf((x - 1.0f) * ZUN_PI * 0.5f) * 0.5f + 0.5f;
        }
        return x;
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
            if (method == INTERP_CONSTANT_VELOCITY || method == INTERP_CONSTANT_ACCEL)
            {
                return initial;
            }
            return goal;
        }
    }
    else if (end_time == 0)
    {
        if (method == INTERP_CONSTANT_VELOCITY || method == INTERP_CONSTANT_ACCEL)
        {
            return initial;
        }
        return goal;
    }
    if (method == INTERP_CONSTANT_VELOCITY)
    {
        // Constant velocity: goal is the step.
        initial += goal;
        current = initial;
        return current;
    }
    else if (method == INTERP_CONSTANT_ACCEL)
    {
        // Constant acceleration: goal is added to the step.
        initial += bezier_2;
        bezier_2 = bezier_2 + goal;
        // Copied as an integer (mov eax; mov) and current reloaded for the
        // return, as in the original; a float assignment forwards xmm0.
        *(i32 *)&current = *(i32 *)&initial;
        return current;
    }
    else if (method == INTERP_BEZIER)
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
DECOMP_NOINLINE void InterpFloat2::reset_timer()
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
            if (method == INTERP_CONSTANT_VELOCITY || method == INTERP_CONSTANT_ACCEL)
            {
                return initial;
            }
            return goal;
        }
    }
    else if (end_time == 0)
    {
        if (method == INTERP_CONSTANT_VELOCITY || method == INTERP_CONSTANT_ACCEL)
        {
            return initial;
        }
        return goal;
    }
    if (method == INTERP_CONSTANT_VELOCITY)
    {
        initial += goal;
        current = initial;
        return current;
    }
    else if (method == INTERP_CONSTANT_ACCEL)
    {
        initial += bezier_2;
        bezier_2 = bezier_2 + goal;
        // Copied as an integer (mov eax; mov) and current reloaded for the
        // return, as in the original; a float assignment forwards xmm0.
        *(i32 *)&current = *(i32 *)&initial;
        return current;
    }
    else if (method == INTERP_BEZIER)
    {
        f32 t = time.current_f / (f32)end_time;
        current = t * t * (3.0f - 2.0f * t) * goal + (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f) * initial +
                  (1.0f - t) * (1.0f - t) * t * bezier_1 + (t - 1.0f) * t * t * bezier_2;
        return current;
    }
    current = interp_common_methods(method, time.current_f, (f32)end_time) * (goal - initial) + initial;
    return current;
}

// The fields are read through a local copy of this, as in InterpFloat3::step.
// TODO: 99%; the timer tick stores current_f in each branch (the original once after both), and initial = bezier_2 + tmp loads bezier_2.x first (the original tmp.x).
// FUNCTION: TH16 0x463d40
HARNESS_CALLED D3DXVECTOR2 InterpFloat2::step()
{
    // Never used: four more named locals put the x components of the vector
    // adds in the original's load order (see InterpFloat3::step).
    i32 unused_0, unused_1, unused_2, unused_3;
    (void)unused_0, (void)unused_1, (void)unused_2, (void)unused_3;
    InterpFloat2 *self = this;
    if (self->end_time > 0)
    {
        self->time.tick_mixed();
        if (self->time.current >= self->end_time)
        {
            self->time.set(self->end_time);
            self->end_time = 0;
            if (self->method == INTERP_CONSTANT_VELOCITY || self->method == INTERP_CONSTANT_ACCEL)
            {
                return self->initial;
            }
            return self->goal;
        }
    }
    else if (self->end_time == 0)
    {
        if (self->method == INTERP_CONSTANT_VELOCITY || self->method == INTERP_CONSTANT_ACCEL)
        {
            return self->initial;
        }
        return self->goal;
    }
    if (self->method == INTERP_CONSTANT_VELOCITY)
    {
        D3DXVECTOR2 tmp = self->initial;
        self->initial = tmp + self->goal;
        self->current = self->initial;
    }
    else if (self->method == INTERP_CONSTANT_ACCEL)
    {
        D3DXVECTOR2 tmp = self->initial;
        self->initial = self->bezier_2 + tmp;
        self->bezier_2 = self->bezier_2 + self->goal;
        self->current = self->initial;
    }
    else if (self->method == INTERP_BEZIER)
    {
        f32 t = self->time.current_f / (f32)self->end_time;
        f32 c_initial = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
        f32 c_goal = t * t * (3.0f - 2.0f * t);
        f32 c_bezier_1 = (1.0f - t) * (1.0f - t) * t;
        f32 c_bezier_2 = (t - 1.0f) * t * t;
        self->current = self->initial * c_initial + self->goal * c_goal + self->bezier_1 * c_bezier_1 +
                        self->bezier_2 * c_bezier_2;
    }
    else
    {
        f32 x = interp_common_methods(self->method, self->time.current_f, (f32)self->end_time);
        self->current = (self->goal - self->initial) * x + self->initial;
    }
    return self->current;
}

// The fields are read through a local copy of this: through this itself LTCG
// gave the bezier terms other registers (1.0f in xmm5, the original's xmm6)
// and took the operands of the adds in another order.
// TODO: 97%; the timer tick adds current_f from memory into the speed's register and stores in each branch (tick() merges the stores but still adds into the speed's register).
// FUNCTION: TH16 0x406e10
D3DXVECTOR3 InterpFloat3::step()
{
    // Never used: one more named local puts the x components of the vector
    // adds in the original's load order (that order follows the function's
    // count of named variables).
    i32 unused;
    (void)unused;
    InterpFloat3 *self = this;
    if (self->end_time > 0)
    {
        self->time.tick_mixed();
        if (self->time.current >= self->end_time)
        {
            self->time.set(self->end_time);
            self->end_time = 0;
            if (self->method == INTERP_CONSTANT_VELOCITY || self->method == INTERP_CONSTANT_ACCEL)
            {
                return self->initial;
            }
            return self->goal;
        }
    }
    else if (self->end_time == 0)
    {
        if (self->method == INTERP_CONSTANT_VELOCITY || self->method == INTERP_CONSTANT_ACCEL)
        {
            return self->initial;
        }
        return self->goal;
    }
    if (self->method == INTERP_CONSTANT_VELOCITY)
    {
        D3DXVECTOR3 tmp = self->initial;
        self->initial = self->goal + tmp;
        self->current = self->initial;
    }
    else if (self->method == INTERP_CONSTANT_ACCEL)
    {
        D3DXVECTOR3 tmp = self->initial;
        self->initial = tmp + self->bezier_2;
        self->bezier_2 = self->bezier_2 + self->goal;
        self->current = self->initial;
    }
    else if (self->method == INTERP_BEZIER)
    {
        f32 t = self->time.current_f / (f32)self->end_time;
        f32 c_initial = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
        f32 c_goal = t * t * (3.0f - 2.0f * t);
        f32 c_bezier_1 = (1.0f - t) * (1.0f - t) * t;
        f32 c_bezier_2 = (t - 1.0f) * t * t;
        self->current = self->initial * c_initial + self->goal * c_goal + self->bezier_1 * c_bezier_1 +
                        self->bezier_2 * c_bezier_2;
    }
    else
    {
        f32 x = interp_common_methods(self->method, self->time.current_f, (f32)self->end_time);
        self->current = (self->goal - self->initial) * x + self->initial;
    }
    return self->current;
}

// TODO: 99%; the same remaining differences as InterpFloat2::step, of which this is a second copy.
// FUNCTION: TH16 0x425570
HARNESS_CALLED D3DXVECTOR2 InterpFloat2::step_radial_dist()
{
    // Never used: four more named locals put the x components of the vector
    // adds in the original's load order (see InterpFloat3::step).
    i32 unused_0, unused_1, unused_2, unused_3;
    (void)unused_0, (void)unused_1, (void)unused_2, (void)unused_3;
    InterpFloat2 *self = this;
    if (self->end_time > 0)
    {
        self->time.tick_mixed();
        if (self->time.current >= self->end_time)
        {
            self->time.set(self->end_time);
            self->end_time = 0;
            if (self->method == INTERP_CONSTANT_VELOCITY || self->method == INTERP_CONSTANT_ACCEL)
            {
                return self->initial;
            }
            return self->goal;
        }
    }
    else if (self->end_time == 0)
    {
        if (self->method == INTERP_CONSTANT_VELOCITY || self->method == INTERP_CONSTANT_ACCEL)
        {
            return self->initial;
        }
        return self->goal;
    }
    if (self->method == INTERP_CONSTANT_VELOCITY)
    {
        D3DXVECTOR2 tmp = self->initial;
        self->initial = tmp + self->goal;
        self->current = self->initial;
    }
    else if (self->method == INTERP_CONSTANT_ACCEL)
    {
        D3DXVECTOR2 tmp = self->initial;
        self->initial = self->bezier_2 + tmp;
        self->bezier_2 = self->bezier_2 + self->goal;
        self->current = self->initial;
    }
    else if (self->method == INTERP_BEZIER)
    {
        f32 t = self->time.current_f / (f32)self->end_time;
        f32 c_initial = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
        f32 c_goal = t * t * (3.0f - 2.0f * t);
        f32 c_bezier_1 = (1.0f - t) * (1.0f - t) * t;
        f32 c_bezier_2 = (t - 1.0f) * t * t;
        self->current = self->initial * c_initial + self->goal * c_goal + self->bezier_1 * c_bezier_1 +
                        self->bezier_2 * c_bezier_2;
    }
    else
    {
        f32 x = interp_common_methods(self->method, self->time.current_f, (f32)self->end_time);
        self->current = (self->goal - self->initial) * x + self->initial;
    }
    return self->current;
}

// The per-axis loop is a do-while (the vector research agent): as a for loop the
// constant acceleration case loaded bezier_2 before initial.
// TODO: 97%; that case stores current from xmm0 where the original copies initial back through eax (an int copy or a bezier_2 store before it gives that but swaps esi and edi everywhere), and the returned vector's z is loaded after x/y.
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
            if (method_for_3d == INTERP_CONSTANT_VELOCITY || method_for_3d == INTERP_CONSTANT_ACCEL)
            {
                return initial;
            }
            return goal;
        }
    }
    else if (end_time == 0)
    {
        if (method_for_3d == INTERP_CONSTANT_VELOCITY || method_for_3d == INTERP_CONSTANT_ACCEL)
        {
            return initial;
        }
        return goal;
    }
    if (!(flag_1d & 1))
    {
        if (method_for_3d == INTERP_CONSTANT_VELOCITY)
        {
            D3DXVECTOR3 tmp = initial;
            initial = tmp + goal;
            current = initial;
        }
        else if (method_for_3d == INTERP_CONSTANT_ACCEL)
        {
            D3DXVECTOR3 tmp = initial;
            initial = bezier_2 + tmp;
            bezier_2 = goal + bezier_2;
            current = initial;
        }
        else if (method_for_3d == INTERP_BEZIER)
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
        i32 i = 0;
        do
        {
            if (methods_1d[i] == INTERP_CONSTANT_VELOCITY)
            {
                initial[i] = goal[i] + initial[i];
                current[i] = initial[i];
            }
            else if (methods_1d[i] == INTERP_CONSTANT_ACCEL)
            {
                // bezier_2 is the velocity here and goal the acceleration;
                // the new velocity is computed before current is set.
                initial[i] = bezier_2[i] + initial[i];
                f32 velocity = bezier_2[i] + goal[i];
                current[i] = initial[i];
                bezier_2[i] = velocity;
            }
            else if (methods_1d[i] == INTERP_BEZIER)
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
        } while (++i < 3);
    }
    return current;
}

// The timer tick is tick_goto: tick() and tick_mixed added current_f from
// memory into the speed's register where the original loads it into xmm0.
// TODO: 100%*: the merged timer stores come current_f first (current first in the original); with the stores after the branches instead, the lerp's y lea takes its operands the other way round.
// FUNCTION: TH16 0x464590
HARNESS_CALLED Int3 InterpInt3::step()
{
    if (end_time > 0)
    {
        time.tick_goto();
        if (time.current >= end_time)
        {
            time.set(end_time);
            end_time = 0;
            if (method == INTERP_CONSTANT_VELOCITY || method == INTERP_CONSTANT_ACCEL)
            {
                return initial;
            }
            return goal;
        }
    }
    else if (end_time == 0)
    {
        if (method == INTERP_CONSTANT_VELOCITY || method == INTERP_CONSTANT_ACCEL)
        {
            return initial;
        }
        return goal;
    }
    if (method == INTERP_CONSTANT_VELOCITY)
    {
        Int3 tmp = initial;
        initial = tmp + goal;
        current = initial;
    }
    else if (method == INTERP_CONSTANT_ACCEL)
    {
        Int3 tmp = initial;
        initial = tmp + bezier_2;
        bezier_2 = goal + bezier_2;
        current = initial;
    }
    else if (method == INTERP_BEZIER)
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

// FUNCTION: TH16 0x464080
HARNESS_CALLED ZunAngle InterpAngle::step()
{
    if (end_time > 0)
    {
        time.tick();
        if (time.current >= end_time)
        {
            time.set(end_time);
            end_time = 0;
            if (method == INTERP_CONSTANT_VELOCITY || method == INTERP_CONSTANT_ACCEL)
            {
                return initial;
            }
            return goal;
        }
    }
    else if (end_time == 0)
    {
        if (method == INTERP_CONSTANT_VELOCITY || method == INTERP_CONSTANT_ACCEL)
        {
            return initial;
        }
        return goal;
    }
    if (method == INTERP_CONSTANT_VELOCITY)
    {
        ZunAngle tmp = initial;
        initial.value = wrap_angle(goal.value + tmp.value);
        current = initial;
        return current;
    }
    else if (method == INTERP_CONSTANT_ACCEL)
    {
        ZunAngle tmp = initial;
        initial.value = wrap_angle(bezier_2.value + tmp.value);
        bezier_2.value = wrap_angle(goal.value + bezier_2.value);
        current = initial;
        return current;
    }
    else if (method == INTERP_BEZIER)
    {
        f32 t = time.current_f / (f32)end_time;
        current = initial * ((t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f))
                + goal * (t * t * (3.0f - 2.0f * t))
                + bezier_1 * ((1.0f - t) * (1.0f - t) * t)
                + bezier_2 * ((t - 1.0f) * t * t);
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
