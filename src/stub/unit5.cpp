// Placeholders for functions and globals outside unit 5's range that its
// code uses. Compiled without /GL, so calls into them stay opaque.
#include <stddef.h>

#include "../AnmManager.h"
#include "../AnmVm.h"
#include "../EffectManager.h"
#include "../FileSystem.h"
#include "../Globals.h"
#include "../Gui.h"
#include "../Item.h"
#include "../Laser.h"
#include "../Supervisor.h"
#include "../ZunTimer.h"

// GLOBAL: TH16 0x490eb0
f32 *const g_timer_speed_ptrs[] = {&g_game_speed, NULL};

// Opaque work for unit 5's placeholder methods (functions in its range that
// are not decompiled yet but must exist in a /GL file).
i32 unit5_placeholder(void *object)
{
    return object != NULL;
}

// STUB: TH16 0x402440
u8 *LTCG_FASTCALL file_read_all(const char *path, i32 *size, i32 not_in_archive)
{
    return NULL;
}


