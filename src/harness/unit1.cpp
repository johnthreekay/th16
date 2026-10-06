// Stand-in callers for unit 1 (0x402e70-0x409490).
#include "../CriticalSections.h"
#include "../ZunMath.h"

// ECL and the per-object updates write g_GameSpeed all the time. Without a
// write LTCG would treat it as the constant 1.0.
void harness_set_game_speed(f32 speed)
{
    g_GameSpeed = speed;
}

// Like the file loader around 0x45ddbb, which leaves CS_FILE this way.
void harness_leave_cs(int i)
{
    g_CriticalSections.leave(CS_FILE);
    g_CriticalSections.leave(i);
}
