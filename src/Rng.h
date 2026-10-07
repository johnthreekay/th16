#pragma once

#include "decomp.h"
#include "types.h"

// TH06's generator, now guarded by a critical section.
struct Rng
{
    u16 seed;
    u32 generation_count;

    // TH06 equivalent: Rng::GetRandomU16
    u16 rand_u16();
    // TH06 equivalent: Rng::GetRandomU16InRange
    __forceinline u16 rand_u16_in_range(u16 range)
    {
        return range != 0 ? rand_u16() % range : 0;
    }
    // TH06 equivalent: Rng::GetRandomU32
    u32 rand_u32();
    // TH06 equivalent: Rng::GetRandomF32ZeroToOne
    f32 randf_0_to_1();
    f32 randf_neg_1_to_1();
    f32 randf_neg_pi_to_pi();
    // TH06 equivalent: Rng::GetRandomF32InRange
    f32 randf_0_to(f32 range)
    {
        return randf_0_to_1() * range;
    }
    f32 randf_neg_to(f32 range)
    {
        return randf_neg_1_to_1() * range;
    }
    // TH06 equivalent: Rng::GetRandomU32InRange
    u32 rand_u32_in_range(u32 range)
    {
        return range != 0 ? rand_u32() % range : 0;
    }
    // randf_neg_1_to_1() * pi. Used by the ECL movement code.
    HARNESS_CALLED f32 randf_neg_1_to_1_times_pi();
};

// Drives effects that replays do not need to reproduce, such as screen
// shake from the ECL (ExpHP: REPLAY_UNSAFE_RNG).
extern Rng g_replay_unsafe_rng;
// The game's RNG, recorded in replays (ExpHP: REPLAY_SAFE_RNG).
extern Rng g_replay_safe_rng;
