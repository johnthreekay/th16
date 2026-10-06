// Stand-in callers for wave 3, range C (0x42b480-0x43dc30), for functions
// whose shape depends on how they are called.
#include "../EnemyManager.h"
#include "../Player.h"

// Like GameThread::thread_start at 0x42d0a9.
void harness_w3c_enemy_manager_reset()
{
    g_EnemyManager->reset_for_stage(0);
}

// Like the game start code at 0x42e171.
void harness_w3c_player_resume_options()
{
    g_Player->resume_options();
}
