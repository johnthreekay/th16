#pragma once

#include "types.h"

// Game speed multipliers a timer can follow, picked by Timer::speed_index.
extern f32 *g_timer_speeds[1];

// ZUN's frame counter (ExpHP: zTimer; TH06: ZunTimer). Bit 0 of control
// says whether initialize() has run.
struct Timer
{
    i32 previous;
    i32 current;
    f32 current_f;
    // Index into g_timer_speeds (ExpHP: game_speed__disused).
    u32 speed_index;
    u32 control;

    Timer()
    {
        control &= ~1;
    }

    void initialize()
    {
        if (!(control & 1))
        {
            current = 0;
            previous = -999999;
            current_f = 0.0f;
            speed_index = 0;
            control |= 1;
        }
    }

    void tick()
    {
        if (speed_index >= 1)
        {
            speed_index = 0;
        }
        f32 *speed = g_timer_speeds[speed_index];
        previous = current;
        if (speed != NULL && (*speed <= 0.99f || *speed >= 1.01f))
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

    void set(i32 value)
    {
        current = value;
        current_f = (f32)value;
        previous = value - 1;
    }
};
