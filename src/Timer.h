#pragma once

#include "types.h"

enum TimerFlags
{
    TIMER_INITIALIZED = 1 << 0,
};

// Frame counter that can run at a fractional speed. Layout from ExpHP's
// th-re-data (zTimer). TH06 equivalent: ZunTimer.
struct Timer
{
    i32 previous;
    i32 current;
    f32 current_f;
    // Index into g_timer_speeds.
    u32 speed_index;
    u32 flags;

    Timer()
    {
        flags &= ~TIMER_INITIALIZED;
    }

    void reset()
    {
        current = 0;
        previous = -999999;
        current_f = 0.0f;
    }

    void initialize()
    {
        reset();
        speed_index = 0;
        flags |= TIMER_INITIALIZED;
    }

    void initialize_if_needed()
    {
        if (!(flags & TIMER_INITIALIZED))
        {
            initialize();
        }
    }

    // TH06 equivalent: ZunTimer::Tick
    void operator++(int);
    void set(i32 value);
};

// Speed of each timer kind, if it can be slowed down.
extern f32 *const g_timer_speeds[1];
