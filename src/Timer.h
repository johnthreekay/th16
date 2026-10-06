#pragma once

#include <stddef.h>

#include "decomp.h"
#include "types.h"

// The speeds a timer can follow, chosen by Timer::speed_index. Only one
// entry: the global game speed.
extern f32 *const g_timer_speeds[1];

enum TimerControl
{
    TIMER_INITIALIZED = 1 << 0,
};

// Frame counter that follows the game speed. ExpHP's zTimer; TH06's
// ZunTimer grown a speed selector.
struct Timer
{
    i32 previous;
    i32 current;
    f32 current_f;
    // Index into g_timer_speeds.
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

    void initialize_if_needed()
    {
        if (!(control & TIMER_INITIALIZED))
        {
            initialize();
        }
    }

    void set(i32 value)
    {
        initialize_if_needed();
        current = value;
        current_f = (f32)value;
        previous = value - 1;
    }

    void operator=(i32 value)
    {
        set(value);
    }

    f32 *speed()
    {
        if (speed_index >= 1)
        {
            speed_index = 0;
        }
        return g_timer_speeds[speed_index];
    }

    void tick()
    {
        f32 *speed = this->speed();
        previous = current;
        if (speed != NULL && !(*speed > 0.99f && *speed < 1.01f))
        {
            current_f += *speed;
            current = (i32)current_f;
        }
        else
        {
            current_f += 1.0f;
            current++;
        }
    }

    // 0x406490
    HARNESS_CALLED void set_value(i32 value);
    // 0x406190. The int is C++'s postfix marker; LTCG drops it but keeps
    // the stack slot.
    HARNESS_CALLED void operator++(int);
    void decrement(f32 n)
    {
        f32 *speed = this->speed();
        previous = current;
        if (speed != NULL && !(*speed > 0.99f && *speed < 1.01f))
        {
            current_f -= *speed * n;
        }
        else
        {
            current_f -= n;
        }
        current = (i32)current_f;
    }

    // 0x40d490
    HARNESS_CALLED void operator--(int);
};
