#pragma once

#include <stddef.h>

#include "decomp.h"
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
// what the out-of-line copies and their inlined uses (set_value at 0x406490,
// operator++ at 0x406190, operator-- at 0x40d490) do.
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

    // Back to frame 0 with no previous frame, leaving speed_index and
    // control alone (AnmVm::wipe uses it right after a memset).
    void clear()
    {
        current = 0;
        previous = -999999;
        current_f = 0.0f;
    }

    void initialize()
    {
        clear();
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

    void operator=(i32 time)
    {
        set(time);
    }

    // 0x406490. The out-of-line copy of set.
    HARNESS_CALLED void set_value(i32 time);

    // Count back by the given number of frames, scaled like tick().
    void operator-=(i32 frames);

    // Advance by a number of frames, scaled like tick().
    void operator+=(f32 frames)
    {
        f32 *speed = this->speed();
        previous = current;
        if (speed != NULL && !(*speed > 0.99f && *speed < 1.01f))
        {
            current_f += *speed * frames;
        }
        else
        {
            current_f += frames;
        }
        current = (i32)current_f;
    }

    // The speed multiplier this timer follows; resets a bad index to the
    // game speed.
    f32 *speed()
    {
        if (speed_index >= 1)
        {
            speed_index = 0;
        }
        return g_timer_speed_ptrs[speed_index];
    }

    // Advance by one frame, scaled by the speed multiplier unless it is
    // close enough to 1.
    void tick()
    {
        f32 *speed = this->speed();
        i32 cur = current;
        f32 cur_f;
        previous = cur;
        if (speed == NULL || (*speed > 0.99f && *speed < 1.01f))
        {
            cur++;
            cur_f = current_f + 1.0f;
        }
        else
        {
            cur_f = current_f + *speed;
            cur = (i32)cur_f;
        }
        current = cur;
        current_f = cur_f;
    }

    // 0x406190. The out-of-line copy of tick. The int is C++'s postfix
    // marker; LTCG drops it but keeps the stack slot.
    HARNESS_CALLED void operator++(int);

    // Count back by n frames, scaled by the speed multiplier unless it is
    // close enough to 1.
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
