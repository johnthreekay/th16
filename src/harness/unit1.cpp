// Stand-in callers for unit 1 (0x402e70-0x409490).
#include "../AnmVm.h"
#include "../AsciiManager.h"
#include "../CriticalSections.h"
#include "../Supervisor.h"
#include "../Interp.h"
#include "../Rng.h"
#include "../ZunMath.h"

// ECL and the per-object updates write g_game_speed all the time. Without a
// write LTCG would treat it as the constant 1.0.
void harness_set_game_speed(f32 speed)
{
    g_game_speed = speed;
}

// Like the file loader at 0x402440 (0x40250b), which leaves CS_FILE this
// way; switch_gamemodes and SoundManager::preload_bgm are the other callers.
void harness_leave_cs(int i)
{
    g_CriticalSections.leave(CS_FILE);
    g_CriticalSections.leave(i);
}

// Like the interpolators' step functions (0x406e10 and others).
f32 harness_interp_common_methods(i32 mode, i32 time, i32 end_time)
{
    return interp_common_methods(mode, (f32)time, (f32)end_time);
}

// Some globals have their address taken elsewhere in the game, so LTCG
// must assume stores through pointers may change them. Without that it
// moves loads of them across such stores.
f32 *harness_screen_coord_scale_ptr()
{
    return &g_screen_coord_scale;
}

// Like the ECL movement code around 0x41ffed.
f32 harness_rand_angle(Rng *rng)
{
    return rng->randf_neg_1_to_1_times_pi() / 3.0f;
}
