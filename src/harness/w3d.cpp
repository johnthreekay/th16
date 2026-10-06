// Stand-in callers for wave 3 range D (0x43dc30-0x44f710: Player,
// PauseMenu, ReplayManager, PopupManager, the title screen) whose shape
// depends on how the rest of the game calls them.
#include "../AnmManager.h"
#include "../ReplayManager.h"
#include "../ZunMath.h"

// Like PauseMenu's snapshot of the game screen (0x43f62c).
i32 harness_w3d_copy_screen(AnmId id, f32 scale)
{
    return g_AnmManager->copy_screen_to_sprite(id, (i32)(scale * 32.0f), (i32)(scale * 16.0f), (i32)(scale * 384.0f),
                                               (i32)(scale * 448.0f));
}

// The camera setup (0x43c858) computes its projection from a half field of
// view.
f32 harness_w3d_tan(f32 *fov, f32 scale)
{
    return fov[1] / zun_tanf(fov[0] * scale);
}

// The pause menu and the replay save menu (0x4406b1, 0x453dda).
i32 harness_w3d_replay_end_stage(i32 stage)
{
    return g_ReplayManager->set_end_stage(stage);
}

// GameThread::thread_start (0x42d027).
void harness_w3d_replay_start_stage()
{
    g_ReplayManager->start_stage();
}

// GameThread's stage setup (0x42dd2d, 0x42df45).
void harness_w3d_replay_begin_stage()
{
    g_ReplayManager->begin_stage();
}
