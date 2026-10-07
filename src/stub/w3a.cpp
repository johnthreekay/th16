// Placeholders for functions and globals that wave 3, range A
// (0x401000-0x4190b0) uses but that are not decompiled yet. Compiled without
// /GL: LTCG cannot see inside, so calls to them stay opaque like calls to
// the real code.
#include "../AnmManager.h"
#include "../BulletManager.h"
#include "../Enemy.h"
#include "../EffectManager.h"

// Defined here so LTCG cannot see that it stays zero.
// GLOBAL: TH16 0x4d9dc4
Float3 g_zero_vec;

// The rows point at the effect kinds' init callbacks (0x4071a0, 0x405670,
// 0x406510, 0x406930), not all decompiled yet. Here so LTCG cannot read the
// values the way it would from a /GL definition.
// GLOBAL: TH16 0x4a2250
EffectData g_effect_table[4];

