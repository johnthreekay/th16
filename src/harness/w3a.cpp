// An escaping address of screen globals (see s6.cpp).
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see docs/workflow.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../AnmManager.h"
#include "../BulletManager.h"
#include "../EffectManager.h"
#include "../Enemy.h"
#include "../Fog.h"
#include "../Gui.h"
#include "../PosVel.h"
#include "../Spellcard.h"
#include "../Supervisor.h"
#include "../ZunMath.h"

// Fog::set_rect (0x418df0) and EnemyData::update_fog (0x41cbd0) reload the
// resolution and the game area's origin inside their loops.
i32 *harness_screen_metric_ptr(i32 which)
{
    switch (which)
    {
    case 0:
        return &g_resolution_x;
    case 1:
        return &g_resolution_y;
    case 2:
        return &g_game_2d_origin_x;
    default:
        return &g_game_2d_origin_y;
    }
}
