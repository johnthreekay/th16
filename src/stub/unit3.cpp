// Placeholders for functions and globals that unit 3 (0x411860-0x41a3f0)
// uses but that are not decompiled yet. Compiled without /GL: LTCG cannot see
// inside, so calls to them stay opaque like calls to the real code.
#include "../AnmManager.h"
#include "../AnmVm.h"
#include "../BulletManager.h"
#include "../Ending.h"
#include "../Spellcard.h"

// GLOBAL: TH16 0x491b0c
AnmVmSwitchFunc g_anm_on_switch_funcs[4];

// GLOBAL: TH16 0x4c0f40
i32 g_unk_4c0f40;

// GLOBAL: TH16 0x4a50b8
u32 g_hardware_input_repeat;

// GLOBAL: TH16 0x4a50bc
u32 g_hardware_input_pressed;

// GLOBAL: TH16 0x491700
i8 g_spell_difficulty[0x78];

// GLOBAL: TH16 0x4a51c4
u32 g_hardware_input_held_4a51c4;

// STUB: TH16 0x417930
i32 Spellcard::on_tick_body()
{
    return 1;
}

// STUB: TH16 0x411e70
i32 Bullet::on_tick()
{
    return 0;
}

