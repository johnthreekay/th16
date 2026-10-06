// Stand-in call sites for unit 4 functions whose shape depends on their
// callers (link-time code generation sees every caller of these).
#include "../Enemy.h"

// Every caller goes through g_EnemyManager, so LTCG replaces `this` with a
// load of the global inside these and drops the parameter (the original
// ecl_run_over_300 instructions for boss handling).
void harness_enemy_manager_boss(int index, EnemyInf *enemy, int value)
{
    g_EnemyManager->set_boss_bit(value);
    g_EnemyManager->set_boss_id(index, enemy);
}
