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
    // Advances by a number of frames at the timer's speed.
    void operator+=(f32 frames);
    void set(i32 value);
};

// Speed of each timer kind, if it can be slowed down.
extern f32 *const g_timer_speeds[1];

inline void Timer::operator+=(f32 frames)
{
    if (speed_index >= 1)
    {
        speed_index = 0;
    }
    f32 *speed = g_timer_speeds[speed_index];
    previous = current;
    if (speed != 0 && !(*speed > 0.99f && *speed < 1.01f))
    {
        current_f += *speed * frames;
    }
    else
    {
        current_f += frames;
    }
    current = (i32)current_f;
}
