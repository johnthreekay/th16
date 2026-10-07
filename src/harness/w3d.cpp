// Stand-in callers for zun_tanf and ReplayManager::set_end_stage.
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see README.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../AnmManager.h"
#include "../MainMenu.h"
#include "../Player.h"
#include "../Supervisor.h"
#include "../ReplayManager.h"
#include "../Scorefile.h"
#include "../ZunMath.h"

// Keeps zun_tanf (0x43dc90) alive. Its only caller in the original is the
// camera setup (0x43c858), which in our source (Supervisor::setup_cameras)
// calls the CRT's tanf instead.
f32 harness_w3d_tan(f32 *fov, f32 scale)
{
    return fov[1] / zun_tanf(fov[0] * scale);
}

// Stands for the pause menu (0x4406b1) and the replay save menu (0x453dda),
// the callers of ReplayManager::set_end_stage (0x4483b0). Both pass 1, which
// our LTCG folds into the function and the original's did not.
i32 harness_w3d_replay_end_stage(i32 stage)
{
    return g_ReplayManager->set_end_stage(stage);
}
