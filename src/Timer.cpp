#include "Timer.h"

#include "Globals.h"

// GLOBAL: TH16 0x490eb0
f32 *const g_timer_speeds[1] = {&g_game_speed};

// FUNCTION: TH16 0x40d490
HARNESS_CALLED void Timer::decrement(i32 n)
{
    f32 *speed = this->speed();
    previous = current;
    if (speed != NULL && !(*speed > 0.99f && *speed < 1.01f))
    {
        current_f -= *speed * (f32)n;
    }
    else
    {
        current_f -= n;
    }
    current = (i32)current_f;
}
