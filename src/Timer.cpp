#include <stddef.h>

#include "Timer.h"
#include "ZunMath.h"

// GLOBAL: TH16 0x490eb0
f32 *const g_timer_speeds[1] = {&g_GameSpeed};

// TODO: ours loads current before the speed pointer, so eax/edx swap roles.
// FUNCTION: TH16 0x406190
void Timer::operator++(int)
{
    if (speed_index >= 1)
    {
        speed_index = 0;
    }
    f32 *speed = g_timer_speeds[speed_index];
    previous = current;
    if (speed != NULL && !(*speed > 0.99f && *speed < 1.01f))
    {
        current_f += *speed;
        current = (i32)current_f;
    }
    else
    {
        current++;
        current_f += 1.0f;
    }
}

// FUNCTION: TH16 0x406490
void Timer::set(i32 value)
{
    initialize_if_needed();
    current = value;
    current_f = (f32)value;
    previous = value - 1;
}
