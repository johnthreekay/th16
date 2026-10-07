#include "ZunTimer.h"

#include "Supervisor.h"

// The speed multipliers a ZunTimer can follow, selected by its
// speed_index: 0 is the game speed; entry 1 (NULL, a fixed speed of 1) is
// never selected, since speed() resets any index of 1 or more to 0.
// GLOBAL: TH16 0x490eb0
f32 *const g_timer_speed_ptrs[] = {&g_game_speed, NULL};

// TODO: the unscaled path adds current_f into the delta register instead of
// loading current_f into xmm0 and adding the delta.
// Counts back by whole frames, scaled by the speed multiplier unless it is
// close enough to 1.
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

// FUNCTION: TH16 0x464d80
HARNESS_CALLED void ZunTimer::set_from(ZunTimer other)
{
    set(other.current);
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
