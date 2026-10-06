#pragma once

#include "decomp.h"
#include "types.h"

// Game speed multipliers a timer can follow; entry 0 points at the global
// game speed. ExpHP: __ptr_GAME_SPEED_MULT_FROM_ECL.
extern f32 *g_game_speed_ptrs[1];

enum TimerControl
{
    // Set once the timer has been reset; the constructor clears it.
    TIMER_INITIALIZED = 1 << 0,
};

// Frame counter that can run at a game speed multiplier. TH06 equivalent:
// ZunTimer. Layout and method names from ExpHP (zTimer).
struct Timer
{
    i32 previous;
    i32 current;
    f32 current_f;
    // Index into g_game_speed_ptrs (ExpHP: game_speed__disused).
    u32 speed_index;
    u32 control;

    Timer()
    {
        control &= ~TIMER_INITIALIZED;
    }

    void initialize()
    {
        if (!(control & TIMER_INITIALIZED))
        {
            current = 0;
            previous = -999999;
            current_f = 0.0f;
            speed_index = 0;
            control |= TIMER_INITIALIZED;
        }
    }

    void set(i32 t)
    {
        initialize();
        current = t;
        current_f = (f32)t;
        previous = t - 1;
    }

    void reset()
    {
        initialize();
        current = 0;
        current_f = 0.0f;
        previous = -1;
    }

    void increment()
    {
        if (speed_index >= 1)
        {
            speed_index = 0;
        }
        f32 *speed = g_game_speed_ptrs[speed_index];
        previous = current;
        if (speed == NULL || (*speed > 0.99f && *speed < 1.01f))
        {
            current++;
            current_f += 1.0f;
        }
        else
        {
            current_f += *speed;
            current = (i32)current_f;
        }
    }
};
