// Stand-in caller for EnemyManager::allocate_new_enemy.
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see docs/workflow.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../Enemy.h"
#include "../EnemyManager.h"

// allocate_new_enemy (0x41aa70) is HARNESS_CALLED so that LTCG sees every
// caller and drops its unused third argument like the original (the stage
// restart code in GameThread then leaves junk in that slot). Its real
// callers all pass g_EnemyManager, which LTCG would then fold into `this`
// as well; the original keeps `this` in ecx, and a call on another object
// keeps it.
EnemyInf *harness_r3b_allocate_enemy(EnemyManager *manager, EnemyCreateParams *params)
{
    return manager->allocate_new_enemy("main", params, 0);
}
