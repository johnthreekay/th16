#include "Timer.h"

#include "Globals.h"

// GLOBAL: TH16 0x490eb0
f32 *const g_timer_speeds[1] = {&g_game_speed};

// TODO: the original keeps a multiply of the speed by 1.0f that our build
// folds away (here and wherever decrement is inlined).
// FUNCTION: TH16 0x40d490
HARNESS_CALLED void Timer::operator--(int)
{
    decrement(1.0f);
}
