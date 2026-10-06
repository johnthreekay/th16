// Placeholders for functions and globals that unit 3 (0x411860-0x41a3f0)
// uses but that are not decompiled yet. Compiled without /GL: LTCG cannot see
// inside, so calls to them stay opaque like calls to the real code.
#include "../AnmManager.h"
#include "../AnmVm.h"
#include "../BulletManager.h"
#include "../Ending.h"
#include "../EnemyManager.h"
#include "../GameThread.h"
#include "../Spellcard.h"
#include "../Timer.h"

// GLOBAL: TH16 0x4c0f48
AnmManager *g_AnmManager;

// GLOBAL: TH16 0x4a6dc0
EnemyManager *g_EnemyManager;

// GLOBAL: TH16 0x4a6dd4
GameThread *g_GameThread;

// GLOBAL: TH16 0x4a5788
f32 g_game_speed;

// GLOBAL: TH16 0x490eb0
f32 *g_game_speed_ptrs[1] = {&g_game_speed};

// GLOBAL: TH16 0x491b0c
AnmVmSwitchFunc g_anm_on_switch_funcs[4];

// GLOBAL: TH16 0x4c0f40
i32 g_unk_4c0f40;

// GLOBAL: TH16 0x4a50b0
u32 g_hardware_input;

// GLOBAL: TH16 0x4a51c4
u32 g_hardware_input_held_4a51c4;

// STUB: TH16 0x4093f0
AnmVm::AnmVm() LTCG_NOTHROW
{
    memset(this, 0, sizeof(AnmVm));
    sprite_id = -1;
    instr_offset = -1;
}

// STUB: TH16 0x45f980
i32 AnmVm::run()
{
    return 0;
}

// STUB: TH16 0x46d020
AnmLoaded *__stdcall AnmManager::preload_anm(i32 slot, const char *path)
{
    return NULL;
}

// STUB: TH16 0x46f1c0
void __stdcall AnmManager::unload_vm(AnmId id)
{
}

// STUB: TH16 0x46f270
void AnmManager::disable_vms_from_anm_file(AnmLoaded *anm)
{
}

// STUB: TH16 0x46efa0
AnmVm *AnmManager::get_vm_with_id(AnmId id)
{
    return NULL;
}

// STUB: TH16 0x46d770
void AnmLoaded::release()
{
}

// STUB: TH16 0x468490
void AnmManager::draw_vm(AnmVm *vm)
{
}

// STUB: TH16 0x417930
i32 Spellcard::on_tick_body()
{
    return 1;
}

// STUB: TH16 0x417d70
i32 Spellcard::on_draw_body()
{
    return 1;
}

// STUB: TH16 0x411e70
i32 Bullet::on_tick()
{
    return 0;
}

// STUB: TH16 0x4124b0
i32 Bullet::sub_4124b0(i32 arg)
{
    return 0;
}

// STUB: TH16 0x4191f0
i32 Ending::initialize()
{
    return 0;
}

// STUB: TH16 0x4197c0
EndingChildF0::EndingChildF0(void *script)
{
}

// STUB: TH16 0x4190b0
EndingChildF0::~EndingChildF0()
{
}

// STUB: TH16 0x4199f0
i32 EndingChildF0::run()
{
    return 0;
}
