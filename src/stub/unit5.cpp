// Placeholders for functions and globals outside unit 5's range that its
// code uses. Compiled without /GL, so calls into them stay opaque.
#include <stddef.h>

#include "../AnmManager.h"
#include "../AnmVm.h"
#include "../FileSystem.h"
#include "../Globals.h"
#include "../Gui.h"
#include "../Laser.h"
#include "../Supervisor.h"
#include "../Timer.h"

// GLOBAL: TH16 0x4a5788
f32 g_game_speed;

// GLOBAL: TH16 0x490eb0
f32 *const g_timer_speed_ptrs[] = {&g_game_speed, NULL};

// GLOBAL: TH16 0x4c0f48
AnmManager *g_AnmManager;

// 0x4093b0. Nothing destroys an AnmVm yet, so the linker drops this.
AnmVm::~AnmVm()
{
}

// STUB: TH16 0x4093f0
AnmVm::AnmVm()
{
}

// STUB: TH16 0x46d020
AnmLoaded *__stdcall AnmManager::preload_anm(i32 slot, const char *name)
{
    return NULL;
}

// Opaque work for unit 5's placeholder methods (functions in its range that
// are not decompiled yet but must exist in a /GL file).
i32 unit5_placeholder(void *object)
{
    return object != NULL;
}

// GLOBAL: TH16 0x4d9d90
i32 g_unk_4d9d90;

// STUB: TH16 0x46d770
void AnmLoaded::release()
{
}

// STUB: TH16 0x43c5b0
i32 __stdcall Supervisor::start_thread(void (*function)(), void *arg)
{
    return 0;
}

// STUB: TH16 0x402440
u8 *LTCG_FASTCALL file_read_all(const char *path, i32 *size, i32 not_in_archive)
{
    return NULL;
}

// GLOBAL: TH16 0x4a6dcc
Gui *g_Gui;

// STUB: TH16 0x42c390
void Gui::update_bombs(i32 bombs, i32 fragments)
{
}
