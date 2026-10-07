// Placeholders for functions and globals that wave 3, range A
// (0x401000-0x4190b0) uses but that are not decompiled yet. Compiled without
// /GL: LTCG cannot see inside, so calls to them stay opaque like calls to
// the real code.
#include "../AnmManager.h"
#include "../BulletManager.h"
#include "../EffectManager.h"

// Written by window setup code that is not decompiled yet.
// GLOBAL: TH16 0x4d9d48
i32 g_arcade_hud_origin_x;
// GLOBAL: TH16 0x4d9d4c
i32 g_arcade_hud_origin_y;

// Defined here so LTCG cannot see that it stays zero.
// GLOBAL: TH16 0x4d9dc4
Float3 g_zero_vec;

// GLOBAL: TH16 0x49f2e0
BulletTypeData g_bullet_types[44];

// The rows point at the effect kinds' init callbacks (0x4071a0, 0x405670,
// 0x406510, 0x406930), not all decompiled yet. Here so LTCG cannot read the
// values the way it would from a /GL definition.
// GLOBAL: TH16 0x4a2250
EffectData g_effect_table[4];

// STUB: TH16 0x46fac0
void AnmManager::serialize_vm_tree(void *buffer, AnmVm *vm, i32 *size)
{
    *size = 0;
}

// STUB: TH16 0x46fc30
AnmId AnmManager::deserialize_vm_tree(void *buffer, AnmVm *parent, i32 *size)
{
    *size = 0;
    return AnmId();
}
