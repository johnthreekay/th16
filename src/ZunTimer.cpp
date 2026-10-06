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
