#pragma once

#include <stddef.h>

#include "decomp.h"
#include "types.h"

// Pointers to the speed multipliers a ZunTimer can follow; entry 0 is the
// game speed. Lives in .rdata, but code still loads it at run time.
// Defined in ZunTimer.cpp.
extern f32 *const g_timer_speed_ptrs[];

// ZunTimer::control bits.
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

    // Frame 0, following the game speed.
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

    // Back to frame 0 (initializing the timer first if it never was), with
    // frame -1 as the previous one so that frame 0 counts as new.
    void reset()
    {
        initialize_if_needed();
        current = 0;
        current_f = 0.0f;
        previous = -1;
    }

    // reset with everything spelled out, for big callers where LTCG inlined
    // it but our build would not.
    __forceinline void reset_inline()
    {
        if (!(control & ZUN_TIMER_INITIALIZED))
        {
            current = 0;
            previous = -999999;
            current_f = 0.0f;
            speed_index = 0;
            control |= ZUN_TIMER_INITIALIZED;
        }
        current = 0;
        current_f = 0.0f;
        previous = -1;
    }

    // Jumps to the given frame, with the frame before it as the previous one.
    void set(i32 time)
    {
        initialize_if_needed();
        current = time;
        current_f = (f32)time;
        previous = time - 1;
    }

    // set(time).
    void operator=(i32 time)
    {
        set(time);
    }

    // set with everything spelled out, for big callers where LTCG inlined
    // it but our build would not (StageInner::run_std).
    __forceinline void set_inline(i32 time)
    {
        if (!(control & ZUN_TIMER_INITIALIZED))
        {
            current = 0;
            previous = -999999;
            current_f = 0.0f;
            speed_index = 0;
            control |= ZUN_TIMER_INITIALIZED;
        }
        current = time;
        current_f = (f32)time;
        previous = time - 1;
    }

    // 0x406490. The out-of-line copy of set().
    HARNESS_CALLED void set_value(i32 time);

    // set to a fractional frame (a curvy laser split off by a bomb).
    void set_f(f32 time)
    {
        initialize_if_needed();
        current = (i32)time;
        current_f = time;
        previous = current - 1;
    }

    // 0x464d80. set() to another timer's current frame; the timer comes by
    // value (ExpHP: Timer::copy).
    HARNESS_CALLED void set_from(ZunTimer other);

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

    // tick with the int frame in a local and current_f updated in each
    // branch, as other callers inline it.
    void tick_mixed()
    {
        f32 *speed = this->speed();
        i32 cur = current;
        previous = cur;
        if (speed == NULL || (*speed > 0.99f && *speed < 1.01f))
        {
            cur++;
            current_f += 1.0f;
        }
        else
        {
            current_f += *speed;
            cur = (i32)current_f;
        }
        current = cur;
    }

    // tick with the updates made in each branch, as some callers inline it.
    void tick_in_place()
    {
        f32 *speed = this->speed();
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

    // 0x40d490. The out-of-line copy of decrement(1).
    HARNESS_CALLED void operator--(int);

    // tick as AnmVm::run has it: current_f is stored in each branch.
    void tick_split()
    {
        f32 *speed = this->speed();
        i32 cur = current;
        previous = cur;
        if (speed != NULL && !(*speed > 0.99f && *speed < 1.01f))
        {
            current_f = *speed + current_f;
            cur = (i32)current_f;
        }
        else
        {
            cur++;
            current_f = current_f + 1.0f;
        }
        current = cur;
    }

    // tick with a missing speed jumping into the whole-frame branch, so each
    // branch keeps its own stores. How the scaled branch adds depends on
    // the caller and can go either way: some get the original's
    // `addss xmm1, [current_f]` (ScreenEffect::on_tick_flash, on_tick_hold),
    // others its load of current_f into xmm0 first (InterpInt3::step,
    // Bullet::step_ex_00 and step_ex_04, EnemyManager::kill_all).
    void tick_goto()
    {
        f32 *speed = this->speed();
        i32 cur = current;
        previous = cur;
        if (speed == NULL)
        {
            goto whole_frame;
        }
        if (*speed > 0.99f && *speed < 1.01f)
        {
        whole_frame:
            cur++;
            current_f = current_f + 1.0f;
        }
        else
        {
            current_f = *speed + current_f;
            cur = (i32)current_f;
        }
        current = cur;
    }

    // tick with the whole-frame step written twice, once for a missing
    // speed and once for a speed close to 1. MSVC loads the 1.0f for the two
    // copies into a register early, then merges them into one block
    // (EnemyManager::update keeps 1.0f in xmm2 across the store before it).
    void tick_nested()
    {
        f32 *speed = this->speed();
        i32 cur = current;
        f32 cur_f;
        previous = cur;
        if (speed != NULL)
        {
            if (*speed > 0.99f && *speed < 1.01f)
            {
                cur++;
                cur_f = current_f + 1.0f;
            }
            else
            {
                cur_f = current_f + *speed;
                cur = (i32)cur_f;
            }
        }
        else
        {
            cur++;
            cur_f = current_f + 1.0f;
        }
        current = cur;
        current_f = cur_f;
    }

    // Count back by whole frames, ignoring the speed multiplier (ANM's
    // wait instruction, which AnmVm::run runs once the time has passed).
    void rewind(i32 frames)
    {
        previous = current;
        current_f -= frames;
        current = (i32)current_f;
    }

    // Whether the timer moved onto a new frame that is a multiple of n
    // (MainMenu's options screen uses it to pace a repeating sound).
    i32 ticked_on_multiple_of(i32 n);
};
