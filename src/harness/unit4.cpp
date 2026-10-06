// Stand-in call sites for unit 4 functions whose shape depends on their
// callers (link-time code generation sees every caller of these).
#include "../AsciiManager.h"
#include "../Enemy.h"
#include "../EnemyManager.h"
#include "../FpsCounter.h"
#include "../Input.h"

// Every caller goes through g_EnemyManager, so LTCG replaces `this` with a
// load of the global inside these and drops the parameter (the original
// ecl_run_over_300 instructions for boss handling).
void harness_enemy_manager_boss(int index, EnemyInf *enemy, int value)
{
    g_EnemyManager->set_boss_bit(value);
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

// Like the game thread's setup, which creates the enemy manager.
void harness_enemy_manager_create(const char *ecl_filename)
{
    EnemyManager::create(ecl_filename);
}

// Like the main loop (0x45ac02, 0x45ae4e, 0x45b060).
void harness_fps_counter_update()
{
    g_FpsCounter->update();
}

// Like the teardown at 0x43b6ad.
void harness_fps_counter_delete()
{
    delete g_FpsCounter;
}

// Like the dialogue skip check at 0x42b043.
int harness_input_hold_time()
{
    return g_InputState.get_hold_time(9) < 20 || g_InputState.get_hold_time(0) < 20;
}
