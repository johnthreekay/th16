#include "ZunTimer.h"

#include "Supervisor.h"

// The speed multipliers a ZunTimer can follow, selected by its
// speed_index. The only entry is the game speed: speed() resets any index of
// 1 or more to 0 (the original's next table, g_ecl_ext_damage_funcs, starts
// 4 bytes later), so the NULL check for a fixed speed of 1 never succeeds.
// GLOBAL: TH16 0x490eb0
f32 *const g_timer_speed_ptrs[] = {&g_game_speed};

// Counts back by whole frames, scaled by the speed multiplier unless it is
// close enough to 1. A missing speed jumps into the unscaled branch (as in
// tick_goto), which gives that branch its own load of current_f.
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
    if (speed == NULL)
    {
        goto unscaled;
    }
    if (*speed > 0.99f && *speed < 1.01f)
    {
    unscaled:
        current_f = current_f + delta;
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
