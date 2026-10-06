#include "ZunTimer.h"

// TODO: the unscaled path adds current_f into the delta register instead of
// loading current_f into xmm0 and adding the delta.
// FUNCTION: TH16 0x43ac80
void ZunTimer::operator-=(i32 frames)
{
    f32 delta = (f32)-frames;
    if (speed_index >= 1)
    {
        speed_index = 0;
    }
    f32 *speed = g_timer_speed_ptrs[speed_index];
    previous = current;
    if (speed == NULL || (*speed > 0.99f && *speed < 1.01f))
    {
        current_f += delta;
        current = (i32)current_f;
    }
    else
    {
        current_f += *speed * delta;
        current = (i32)current_f;
    }
}

// TODO: the original keeps a multiply of the speed by 1.0f that our build
// folds away (here and wherever decrement is inlined).
// FUNCTION: TH16 0x40d490
HARNESS_CALLED void ZunTimer::operator--(int)
{
    decrement(1.0f);
}

// FUNCTION: TH16 0x406190
HARNESS_CALLED void ZunTimer::operator++(int)
{
    tick();
}

// FUNCTION: TH16 0x406490
HARNESS_CALLED void ZunTimer::set_value(i32 time)
{
    set(time);
}

// FUNCTION: TH16 0x410100
i32 ZunTimer::ticked_on_multiple_of(i32 n)
{
    if (current != previous && current % n == 0)
    {
        return 1;
    }
    return 0;
}
