// Stand-in callers for the wave 4 interpreter work (EnemyData's ECL
// instructions and fog).
#include "../Supervisor.h"

// The window setup code takes the arcade offset's address (like the
// resolution globals in harness/w3a.cpp), so stores through float pointers
// may change it: EnemyData::update_fog reloads it after each store.
i32 *harness_w4a_arcade_offset()
{
    return &g_early_arcade_offset_x;
}
