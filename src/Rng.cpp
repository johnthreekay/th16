#include "CriticalSections.h"
#include "Rng.h"
#include "ZunMath.h"

// FUNCTION: TH16 0x402b70
u16 Rng::rand_u16()
{
    ENTER_CS(CS_RNG);
    generation_count++;
    u16 a = (seed ^ 0x9630) - 0x6553;
    seed = (((a & 0xc000) >> 14) + a * 4) & 0xffff;
    LEAVE_CS(CS_RNG);
    return seed;
}

// FUNCTION: TH16 0x402be0
u32 Rng::rand_u32()
{
    // Unlike TH06, the halves are the intermediate values before each
    // rotation, not the new seeds.
    ENTER_CS(CS_RNG);
    generation_count++;
    u16 a = (seed ^ 0x9630) - 0x6553;
    seed = (((a & 0xc000) >> 14) + a * 4) & 0xffff;
    generation_count++;
    u16 b = (seed ^ 0x9630) - 0x6553;
    seed = (((b & 0xc000) >> 14) + b * 4) & 0xffff;
    LEAVE_CS(CS_RNG);
    return a << 16 | b;
}

// FUNCTION: TH16 0x402c70
f32 Rng::randf_0_to_1()
{
#ifdef TH16_PORT
    port_finit();
#else
    __asm finit;
#endif
    return (f32)rand_u32() / (f32)0xffffffff;
}

// FUNCTION: TH16 0x402cb0
f32 Rng::randf_neg_1_to_1()
{
#ifdef TH16_PORT
    port_finit();
#else
    __asm finit;
#endif
    return (f32)rand_u32() / (f32)0x7fffffff - 1.0f;
}

// FUNCTION: TH16 0x402cf0
f32 Rng::randf_neg_pi_to_pi()
{
#ifdef TH16_PORT
    port_finit();
#else
    __asm finit;
#endif
    return (f32)rand_u32() / ((f32)0xffffffff / ZUN_2PI) - ZUN_PI;
}

// FUNCTION: TH16 0x406320
HARNESS_CALLED f32 Rng::randf_neg_1_to_1_times_pi()
{
    return randf_neg_1_to_1() * ZUN_PI;
}

// GLOBAL: TH16 0x4a6d80
Rng g_replay_unsafe_rng;
// GLOBAL: TH16 0x4a6d88
Rng g_replay_safe_rng;
