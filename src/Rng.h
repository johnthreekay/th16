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
    // TH06 equivalent: Rng::GetRandomU32
    u32 rand_u32();
    // TH06 equivalent: Rng::GetRandomF32ZeroToOne
    f32 randf_0_to_1();
    f32 randf_neg_1_to_1();
    f32 randf_neg_pi_to_pi();
    // randf_neg_1_to_1() * pi. Used by the ECL movement code.
    HARNESS_CALLED f32 randf_neg_1_to_1_times_pi();
};
