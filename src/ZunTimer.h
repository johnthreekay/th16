#pragma once

#include <stddef.h>

#include "types.h"

// Pointers to the speed multipliers a ZunTimer can follow; entry 0 is the
// game speed. Lives in .rdata, but code still loads it at run time.
extern f32 *const g_timer_speed_ptrs[];

enum ZunTimerControl
{
    ZUN_TIMER_INITIALIZED = 1 << 0,
};

// A frame counter that follows the game speed, advancing by fractional
// frames when it changes. Layout from ExpHP's th-re-data (zTimer); TH06's
// ZunTimer is the same idea without the control word. The inline helpers are
// what the out-of-line copies and their inlined uses (set at 0x406490,
// operator++ at 0x406190) do.
struct ZunTimer
{
    i32 previous;
    i32 current;
    f32 current_f;
    // Index into g_timer_speed_ptrs (ExpHP: game_speed__disused).
    u32 speed_index;
    u32 control;

    ZunTimer()
    {
        control &= ~ZUN_TIMER_INITIALIZED;
    }

    void initialize()
    {
        current = 0;
        previous = -999999;
        current_f = 0.0f;
        speed_index = 0;
        control |= ZUN_TIMER_INITIALIZED;
    }

    void initialize_if_needed()
    {
        if (!(control & ZUN_TIMER_INITIALIZED))
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
        current_f = (f32)time;
        previous = time - 1;
    }

    // Count back by the given number of frames, scaled like tick().
    void operator-=(i32 frames);

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
