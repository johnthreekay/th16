// Stand-in callers for unit 5's HARNESS_CALLED functions, shaped like the
// original call sites (ECL instructions around 0x4216c8-0x4222b5, bombs
// around 0x40eb98 and the player around 0x442669).
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

// The game reads g_ItemManager in many places; without a reader LTCG drops
// the stores to it.
ItemManager *harness_item_manager()
{
    return g_ItemManager;
}

// Like LaserCurveInf's run_ex (0x438d89), which appends with an int field
// converted to float.
LaserCurveNode *harness_laser_curve_append(LaserCurveInf *curve, i32 n)
{
    curve->append_node((f32)n);
    return curve->append_node((f32)(n + 1));
}

// Like the player's two timers around 0x445649.
void harness_timer_sub(ZunTimer *a, ZunTimer *b)
{
    *a -= 0xe;
    *b -= 0x77;
}

