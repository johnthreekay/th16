#pragma once

#include <stddef.h>

#include "types.h"

// A frame counter that can advance by fractional frames when the game speed
// changes. Layout from ExpHP's th-re-data (zTimer); TH06's ZunTimer is the
// same idea without the control word.
struct ZunTimer
{
    i32 previous;
    i32 current;
    f32 current_f;
    // Unused in TH16.
    f32 *game_speed;
    // Bit 0: initialized.
    u32 control;

    ZunTimer()
    {
        control &= ~1;
    }

    void initialize()
    {
        current = 0;
        previous = -999999;
        current_f = 0.0f;
        game_speed = NULL;
        control |= 1;
    }

    void initialize_if_needed()
    {
        if (!(control & 1))
        {
            initialize();
        }
    }

    void reset()
    {
        initialize_if_needed();
        current = 0;
        current_f = 0.0f;
        previous = -1;
    }

    void set(i32 time)
    {
        initialize_if_needed();
        current = time;
        current_f = time;
        previous = time - 1;
    }
};
