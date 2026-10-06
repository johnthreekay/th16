// Stand-in callers for unit 5's HARNESS_CALLED functions, shaped like the
// original call sites (ECL instructions around 0x4216c8-0x4222b5, bombs
// around 0x40eb98 and the player around 0x442669).
#include <string.h>

#include "../AnmVm.h"
#include "../GameThread.h"
#include "../Item.h"
#include "../Laser.h"

i32 harness_laser_cancel(Float3 *a, Float3 *b, f32 angle, f32 radius, i32 mode)
{
    i32 n = g_LaserManager->cancel_in_rectangle(a, b, angle, 5, 1);
    n += g_LaserManager->cancel_in_radius(a, radius, 0, 1);
    n += g_LaserManager->cancel_in_radius(a, radius, 1, 1);
    n += g_LaserManager->cancel_in_radius(b, radius, 0, 0);
    g_LaserManager->clear_all(1, 0);
    g_LaserManager->clear_all(mode, 0);
    return n;
}

// Like the game mode switch around 0x43cf36, which starts a game (0) or a
// replay (1).
void harness_create_game_thread(i32 replay)
{
    if (replay)
    {
        GameThread::create(1);
    }
    else
    {
        GameThread::create(0);
    }
}

// Stand-in for AnmVm's constructor (0x4093f0, another unit's). It has to be
// visible to LTCG: owners compile EH cleanup around it that LTCG removes
// only once it can see the constructor does not throw, leaving the spilled
// this behind as in the original.
DECOMP_NOINLINE AnmVm::AnmVm()
{
    memset(this, 0, sizeof(AnmVm));
}

// The game reads g_ItemManager in many places; without a reader LTCG drops
// the stores to it.
ItemManager *harness_item_manager()
{
    return g_ItemManager;
}
