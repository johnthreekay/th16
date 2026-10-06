#pragma once

#include "types.h"

// ZUN's frame counter (ExpHP: zTimer; TH06: ZunTimer). Bit 0 of control
// says whether initialize() has run.
struct Timer
{
    i32 previous;
    i32 current;
    f32 current_f;
    f32 *game_speed;
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
            game_speed = NULL;
            control |= 1;
        }
    }

    void set(i32 value)
    {
        current = value;
        current_f = (f32)value;
        previous = value - 1;
    }
};
