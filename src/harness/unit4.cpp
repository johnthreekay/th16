// Stand-in call sites for unit 4 functions whose shape depends on their
// callers (link-time code generation sees every caller of these).
#include "../AsciiManager.h"
#include "../Enemy.h"
#include "../EnemyManager.h"
#include "../Input.h"

// Every caller goes through g_EnemyManager, so LTCG replaces `this` with a
// load of the global inside these and drops the parameter (the original
// ecl_run_over_300 instructions for boss handling).
void harness_enemy_manager_boss(int index, EnemyInf *enemy, int value)
{
    g_EnemyManager->set_life_bar_hidden(value);
    g_EnemyManager->set_boss_id(index, enemy);
}

// Like EnemyInf::~EnemyInf (0x41ba10).
void harness_enemy_remove(EnemyInf *enemy)
{
    g_EnemyManager->remove_from_active_list(enemy);
}

// Like the callers at 0x41072d and 0x445f36.
int harness_enemy_find_closest(D3DXVECTOR3 *pos, f32 max_dist)
{
    return g_EnemyManager->find_closest(pos, max_dist).id;
}

