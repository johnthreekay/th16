#pragma once

#include <stddef.h>

#include "types.h"

// Pointers to the speed multipliers a Timer can follow; entry 0 is the game
// speed. Lives in .rdata, but code still loads it at run time.
extern f32 *const g_timer_speed_ptrs[];

enum TimerControl
{
    TIMER_INITIALIZED = 1 << 0,
};

// Frame counter that follows the game speed. Layout from ExpHP's th-re-data
// (zTimer); the inline helpers are what the out-of-line copies and their
// inlined uses (Timer::set_value at 0x406490, operator++ at 0x406190) do.
struct Timer
{
    i32 previous;
    i32 current;
    f32 current_f;
    // Index into g_timer_speed_ptrs (ExpHP: game_speed__disused).
    u32 speed_index;
    u32 control;

    Timer()
    {
        control &= ~TIMER_INITIALIZED;
    }

    void initialize()
    {
        current = 0;
        previous = -999999;
        current_f = 0.0f;
        speed_index = 0;
        control |= TIMER_INITIALIZED;
    }

    void set(i32 value)
    {
        if (!(control & TIMER_INITIALIZED))
        {
            initialize();
        }
        current = value;
        current_f = (f32)value;
        previous = value - 1;
    }

    void reset()
    {
        if (!(control & TIMER_INITIALIZED))
        {
            initialize();
        }
        current = 0;
        current_f = 0.0f;
        previous = -1;
    }

    // Advance by one frame, scaled by the speed multiplier unless it is
    // close enough to 1.
    void tick()
    {
        if (speed_index >= 1)
        {
            speed_index = 0;
        }
        f32 *speed = g_timer_speed_ptrs[speed_index];
        previous = current;
        if (speed == NULL || (*speed > 0.99f && *speed < 1.01f))
        {
            current_f += 1.0f;
            current++;
        }
        else
        {
            current_f += *speed;
            current = (i32)current_f;
        }
    }
};
